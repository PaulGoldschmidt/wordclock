// Web Serial connection to the clock: handshake, commands and parsing of the firmware's output.
// Works with the original firmware (6.1) and with 6.2. No DOM access, so the module also runs
// outside the browser (tests).
//
// Firmware protocol (WordCore/communication_selfcheck.ino):
//  - Opening the port resets the Arduino. If a byte arrives during setup(), the clock skips the startup
//    animation, limits the brightness and prints debug output ("PC mode").
//  - A command is only read once at least 13 bytes are buffered; the clock then reads until nothing
//    has arrived for 2.5 s. Two commands must therefore never be sent back to back.
//    Every byte that arrives during the read extends it; precise time setting relies on this.
//  - In PC mode every loop() pass prints "Warte auf neue Befehle..." right before this check.

const BAUD = 115200;
const HELLO = 'PC2ARDUINO';
const READ_TIMEOUT = 2500; // firmware's Serial.setTimeout() for commands, ms
const CMD_LEN = 13;

// Timing for setTime(), in ms
const SET_LAG = READ_TIMEOUT + 100; // last byte received -> RTC written: readString() timeout, then delay(100)
const ECHO_LAG = 2; // the echo line is fully sent just over 1 ms after the RTC write, plus USB latency
const MAX_GAP = READ_TIMEOUT - 300; // largest safe gap between two bytes while the clock is reading
const KEEP_GAP = 1500; // interval of the filler '_' bytes that keep the read open
const AIM_AFTER_READ = 1300; // PC mode: planned time from read start to last byte (if missed, see retry in setTime)
const AIM_BLIND = 2600; // no debug output: command sent -> last byte (the clock starts reading within one loop() pass)
const WARTE = 'Warte auf neue Befehle';
const STATUS_END = 'STATUS END';
const encoder = new TextEncoder();

// Startup messages whose arrival time is recorded in _seen (6.1 calls itself Lichtbild / LightPicture)
const MARKERS = [
  ['banner', /^--(Wordclock|Lichtbild) Startvorgang--/], ['springe', /^Springe Startup-Animation/], ['hi', /^HI$/],
  ['hello', /^Hello World - this is (Wordclock|LightPicture)/], ['eeprom', /^EEPROM ausgelesen/],
];

// Startup output values that 6.1 omits from the status or reports wrongly
const BOOT_KEYS = { Nummer: 'number', 'Tag max. Helligkeit': 'dayMax', 'Tag min. Helligkeit': 'dayMin', 'LDR-Teiler': 'ldr' };

export const now = () => performance.now();
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const pad = (cmd) => cmd.padEnd(CMD_LEN, '_');
export const two = (n) => String(n).padStart(2, '0');
const three = (n) => String(n).padStart(3, '0');

// With automatic DST switching on, the clock expects standard time (CET = UTC+1) in the RTC and
// switches to summer time itself. With switching off, it gets German local time.
const berlinFmt = new Intl.DateTimeFormat('de-DE', {
  timeZone: 'Europe/Berlin', year: 'numeric', month: 'numeric', day: 'numeric',
  hour: 'numeric', minute: 'numeric', second: 'numeric', hourCycle: 'h23',
});

export function berlinFields(ms) {
  const p = Object.fromEntries(berlinFmt.formatToParts(new Date(ms)).map((x) => [x.type, Number(x.value)]));
  return { y: p.year, mo: p.month, d: p.day, h: p.hour, mi: p.minute, s: p.second };
}

function utcFields(ms) {
  const t = new Date(ms);
  return {
    y: t.getUTCFullYear(), mo: t.getUTCMonth() + 1, d: t.getUTCDate(),
    h: t.getUTCHours(), mi: t.getUTCMinutes(), s: t.getUTCSeconds(),
  };
}

// Inverse of utcFields: fields interpreted as UTC -> ms
export const fieldsToMs = (f) => Date.UTC(f.y, f.mo - 1, f.d, f.h, f.mi, f.s);

export const cetFields = (ms) => utcFields(ms + 3600e3);

// German local time (e.g. from an input field) -> timestamp in ms
export function berlinToMs(y, mo, d, h, mi, s = 0) {
  const guess = fieldsToMs({ y, mo, d, h, mi, s });
  for (const off of [1, 2]) {
    const ms = guess - off * 3600e3;
    const f = berlinFields(ms);
    if (f.y === y && f.mo === mo && f.d === d && f.h === h && f.mi === mi) return ms;
  }
  return guess - 3600e3; // nonexistent local time (spring-forward gap)
}

// Years the firmware accepts as a valid RTC time (RTC_Check in RTC_Module.ino)
const validYear = (y) => y >= 2016 && y <= 2099;

export const commands = {
  time: (f) => `@${two(f.y % 100)}${two(f.mo)}${two(f.d)}${two(f.h)}${two(f.mi)}${two(f.s)}`,
  color: ([r, g, b]) => `#/${three(r)}${three(g)}${three(b)}`,
  mode: (m) => ({ 1: '#%', 2: '#=', 3: '#!' })[m],
  brightness: (s) => `!${three(s.dayMin)}${three(s.dayMax)}${three(s.nightMin)}${three(s.nightMax)}${two(s.nightBegin)}${two(s.nightEnd)}`,
  timechange: (on) => `+${on ? 1 : 0}`,
  ldr: (n) => `?${n}`,
  status: () => '&',
  selftest: () => '=',
  factoryReset: () => '*',
};

export class WordCore extends EventTarget {
  constructor(port) {
    super();
    this.port = port;
    this.state = 'closed'; // closed | connecting | setup | ready | limited
    this.firmware = null; // '6.1' | '6.2' | ...
    this.legacy = false; // original firmware 6.1: no extra status lines, different night logic
    this.boot = {}; // values from the startup output
    this.status = null; // parsed reply to '&'
    this.telemetry = {}; // values from the debug output of each loop() pass
    this.rtcError = false;
    this.busy = false;
    this.busyLabel = null;
    this._waiters = new Set();
    this._queue = Promise.resolve();
    this._warteTimes = [];
    this._statusLines = null;
    this._lastLoopLine = 0;
    this._lastWarte = 0;
    this._seen = {}; // MARKERS key -> time (now()) it was last seen
    this._writeLag = SET_LAG + 3; // last byte sent -> RTC written, ms; re-measured on every time set
  }

  // The clock's time, extrapolated from the latest debug line (null while unknown or invalid)
  get clock() {
    const c = this.telemetry.clock;
    if (!c || this.rtcError) return null;
    return utcFields(fieldsToMs(c) + (Date.now() - c.at));
  }

  _emit(type, detail) { this.dispatchEvent(new CustomEvent(type, { detail })); }
  _setState(s) {
    if (this.state === s) return;
    this.state = s;
    this._warteTimes = []; // re-measure the loop period per state (the LED test cycles differently from loop())
    this._lastWarte = 0;
    this._emit('state', s);
  }
  _log(text, dir = 'info') { this._emit('log', { text, dir }); }

  // Resolves with the first non-undefined result of test(line, t) for lines arriving from now on.
  waitFor(test, timeout, what = 'Antwort') {
    return new Promise((resolve, reject) => {
      const w = { test, resolve, reject };
      w.timer = setTimeout(() => { this._waiters.delete(w); reject(new Error(`Keine ${what} von der Uhr (Zeitüberschreitung)`)); }, timeout);
      this._waiters.add(w);
    });
  }

  // Like waitFor, but also resolves if one of the markers already arrived since `since`. Needed because
  // several lines can arrive in the same USB packet and are processed synchronously.
  waitSeen(keys, since, timeout, what) {
    const hit = () => keys.find((k) => this._seen[k] >= since);
    const h = hit();
    return h ? Promise.resolve(h) : this.waitFor(() => hit(), timeout, what);
  }

  // Time of the next "Warte" line after `after`; the clock checks for a command right after printing it
  _nextWarte(after = 0) {
    return this.waitFor((l, t) => (l.startsWith(WARTE) && t > after ? t : undefined), 9000, 'Reaktion');
  }

  _failWaiters(err) {
    for (const w of this._waiters) { clearTimeout(w.timer); w.reject(err); }
    this._waiters.clear();
  }

  async open() {
    this._setState('connecting');
    this._closing = false;
    await this.port.open({ baudRate: BAUD, bufferSize: 4096 });
    this._readLoop();
    try {
      await this._handshake();
    } catch (e) {
      await this.close(false);
      throw e;
    }
  }

  // restart: reset the clock before disconnecting so it runs on without PC mode (full brightness)
  async close(restart = true) {
    if (this.state === 'closed' && !this.port.readable) return;
    this._closing = true;
    if (restart && this.port.writable) {
      try { await this._resetBoard(); } catch { /* port may already be gone */ }
    }
    try { await this._reader?.cancel(); } catch { /* ignore */ }
    await this._readDone;
    try { await this.port.close(); } catch { /* ignore */ }
    this._failWaiters(new Error('Verbindung getrennt'));
    this._setState('closed');
  }

  async _write(text, log = text !== HELLO) {
    const writer = this.port.writable.getWriter();
    try {
      await writer.write(encoder.encode(text));
    } finally {
      writer.releaseLock();
    }
    if (log) this._log(text, 'out');
  }

  _readLoop() {
    this._readDone = (async () => {
      const decoder = new TextDecoder();
      let buf = '';
      while (this.port.readable && !this._closing) {
        this._reader = this.port.readable.getReader();
        try {
          for (;;) {
            const { value, done } = await this._reader.read();
            if (done) break;
            buf += decoder.decode(value, { stream: true });
            let i;
            while ((i = buf.indexOf('\n')) >= 0) {
              const line = buf.slice(0, i).replace(/\r$/, '');
              buf = buf.slice(i + 1);
              this._onLine(line);
            }
            if (buf.length > 4096) buf = buf.slice(-1024); // discard garbage that has no line break
          }
        } catch (e) {
          if (!this._closing) this._log(`Lesefehler: ${e.message}`, 'error');
        } finally {
          this._reader.releaseLock();
          this._reader = null;
        }
      }
      if (!this._closing) { // connection lost, e.g. cable unplugged
        this._failWaiters(new Error('Die Verbindung zur Uhr wurde unterbrochen'));
        this._setState('closed');
        this._emit('lost');
      }
    })();
  }

  // Reset via DTR like the Arduino IDE: the off -> on edge triggers the reset through the capacitor.
  async _resetBoard() {
    await this.port.setSignals({ dataTerminalReady: false, requestToSend: false });
    await sleep(120);
    await this.port.setSignals({ dataTerminalReady: true, requestToSend: true });
  }

  async _handshake(retried = false) {
    this._setState('connecting');
    this.boot = {};
    this.status = null;
    this.telemetry = {};
    this._lastLoopLine = 0;
    this.rtcError = false;
    const t0 = now();
    await this._resetBoard();

    // Send HELLO repeatedly until the startup banner: bytes arriving while the bootloader runs are lost.
    let banner = null;
    const bannerP = this.waitSeen(['banner'], t0, 5000, 'Startmeldung').catch(() => null);
    bannerP.then((b) => { banner = b; });
    const spamUntil = now() + 4000;
    while (!banner && now() < spamUntil) {
      await this._write(HELLO);
      await Promise.race([sleep(100), bannerP]);
    }
    if (!(await bannerP)) return this._probeWithoutReset();
    const bannerAt = this._seen.banner;

    const result = await this.waitSeen(['springe', 'hello', 'hi'], bannerAt, 3000, 'Startmeldung').catch(() => 'timeout');
    if (result === 'hello') { // new clock: EEPROM not initialised, waiting for the time
      this._setState('setup');
      // In the LED test the clock only reads once 13 bytes are buffered, so leftover HELLO bytes would
      // stay in the buffer and corrupt the first command. A filler command flushes them out as an invalid one.
      await this.command('', { label: 'Verbindung vorbereiten' });
      return undefined;
    }
    if (result !== 'springe') {
      // The clock missed the HELLO and is playing its startup animation: retry once.
      if (!retried) {
        this._log('Die Uhr hat den Computer beim Start nicht erkannt, neuer Versuch …', 'info');
        return this._handshake(true);
      }
      await this.waitSeen(['hi'], bannerAt, 25000, 'Startmeldung').catch(() => {});
      this._setState('limited');
      await this.refreshStatus().catch(() => {});
      return undefined;
    }
    await this.waitSeen(['hi'], bannerAt, 4000, 'Startmeldung');
    this._lastLoopLine = now(); // the deadline for the first debug line (RTC error detection) starts now
    this._setState('ready');
    await this.refreshStatus();
    return undefined;
  }

  // No startup banner: the clock did not reset on connect (e.g. no DTR line).
  async _probeWithoutReset() {
    await sleep(READ_TIMEOUT + 800); // the clock may be reading the HELLO bytes as a command; let that read end
    this._setState('limited');
    try {
      await this.refreshStatus();
    } catch {
      throw new Error('Die Uhr antwortet nicht. Ist der richtige Anschluss gewählt und die Uhr mit Strom versorgt?');
    }
  }

  _onLine(line) {
    const t = now();
    this._log(line, 'in');
    let m;
    const marker = MARKERS.find(([, re]) => re.test(line));
    if (marker) this._seen[marker[0]] = t;
    if (this._statusLines) {
      if (line === STATUS_END) {
        this._finishStatus(this._statusLines);
        this._statusLines = null;
      } else {
        this._statusLines.push(line);
      }
    } else if (line.endsWith('STATUS BEGIN')) {
      this._statusLines = [];
    } else if (line.startsWith(WARTE)) {
      // Record the loop period, but not across a command read (>= 2.5 s)
      const gap = t - this._lastWarte;
      if (this._lastWarte && gap > 900 && gap < 2300) this._warteTimes = [...this._warteTimes.slice(-7), gap];
      this._lastWarte = t;
      // On an RTC error 6.2 prints no debug output: "Warte" without "Loop durchlaufen" means RTC error
      if (this.state === 'ready' && this._lastLoopLine && t - this._lastLoopLine > 6000 && !this.busy) this.rtcError = true;
    } else if ((m = line.match(/^Loop durchlaufen am (\d+)\.(\d+)\.(\d+) um (\d+):(\d+):(\d+)/))) {
      const [d, mo, y, h, mi, s] = m.slice(1).map(Number);
      this._lastLoopLine = t;
      this.telemetry.clock = { y, mo, d, h, mi, s, at: Date.now() };
      this.rtcError = !validYear(y);
    } else if ((m = line.match(/^Die aktuelle Helligkeit der Uhr betraegt (\d+), wobei der LDR den Wert (\d+) hat/))) {
      this.telemetry.brightness = Number(m[1]);
      this.telemetry.ldr = Number(m[2]);
    } else if ((m = line.match(/^(Nummer|Tag max\. Helligkeit|Tag min\. Helligkeit|LDR-Teiler): (\d+)/))) {
      this.boot[BOOT_KEYS[m[1]]] = Number(m[2]);
    }
    for (const w of [...this._waiters]) {
      let r;
      try { r = w.test(line, t); } catch { r = undefined; }
      if (r !== undefined) {
        clearTimeout(w.timer);
        this._waiters.delete(w);
        w.resolve(r);
      }
    }
  }

  _finishStatus(lines) {
    const nums = [];
    const extra = {};
    for (const l of lines) {
      const kv = l.match(/^([A-Z]+)=(.*)$/);
      if (kv) extra[kv[1]] = kv[2].trim();
      else if (/^-?\d+$/.test(l.trim())) nums.push(Number(l.trim()));
    }
    if (nums.length < 13) return;
    const [dayMax, dayMin, nightMax, nightMin, nightBegin, nightEnd, r, g, b, mode, timechange, startups, runtime] = nums;
    this.legacy = extra.VERSION === undefined; // 6.1 sends no NAME=value lines
    this.firmware = extra.VERSION || '6.1';
    this.status = {
      // In the colour cycle modes 6.1 reports the last drawn colour, 6.2 always the stored fixed colour
      dayMax, dayMin, nightMax, nightMin, nightBegin, nightEnd, color: this.legacy && mode !== 0 ? null : [r, g, b], mode, timechange,
      startups, runtime,
      ldr: extra.LDR !== undefined ? Number(extra.LDR) : this.boot.ldr,
      number: extra.NR !== undefined ? Number(extra.NR) : this.boot.number,
    };
    // In PC mode 6.1 reports the temporarily limited brightness (39/40) instead of the stored values.
    // The day values come from the startup output; the night values cannot be read (null).
    if (this.legacy && this.state !== 'limited') {
      this.status.dayMax = this.boot.dayMax ?? null;
      this.status.dayMin = this.boot.dayMin ?? null;
      this.status.nightMax = null;
      this.status.nightMin = null;
    }
    this._emit('status', this.status);
  }

  get loopPeriod() {
    const w = this._warteTimes;
    if (!w.length) return this.legacy ? 1800 : 1550;
    return w.slice().sort((a, b) => a - b)[Math.floor(w.length / 2)];
  }

  get debugOutput() { return this.state === 'ready' || this.state === 'setup'; }

  // Queues a command. It is only sent once the clock has finished reading the previous one.
  command(cmd, { expect, timeout = 12000, label } = {}) {
    return this._enqueue(() => this._run(cmd, { expect, timeout, label }));
  }

  _enqueue(job) {
    const p = this._queue.then(job);
    this._queue = p.catch(() => {});
    return p;
  }

  _setBusy(busy, label = null) {
    this.busy = busy;
    this.busyLabel = label;
    this._emit('busy', busy);
  }

  // beforeSend(): runs before sending (e.g. to wait for the right moment).
  // afterRead(readAt, sentAt): runs after sending while the clock reads; returns the time the last byte
  // was sent. readAt = observed start of the read, or null without debug output.
  async _run(cmd, { expect, timeout, label, beforeSend, afterRead }) {
    if (this.state === 'closed' || this.state === 'connecting') throw new Error('Nicht verbunden');
    this._setBusy(true, label);
    try {
      if (beforeSend) await beforeSend();
      const text = typeof cmd === 'function' ? cmd() : cmd;
      const sentAt = now();
      // The first "Warte" line certainly printed after our bytes arrived marks the moment the clock
      // reads the command.
      const readP = this.debugOutput ? this._nextWarte(sentAt + 40) : null;
      const expectP = expect ? this.waitFor(expect, timeout, 'Antwort') : null;
      readP?.catch(() => {});
      expectP?.catch(() => {});
      await this._write(pad(text));
      let readAt = readP ? await readP.catch(() => null) : null;
      this._assertOpen();
      if (readAt) this._emit('reading', { cmd: text, at: readAt });
      const lastByteAt = afterRead ? await afterRead(readAt, sentAt) : sentAt;
      this._assertOpen();
      const result = expectP ? await expectP : undefined;
      if (!readAt) readAt = sentAt + (this.debugOutput ? this.loopPeriod : 2500);
      // The command is only complete once the clock has received nothing for 2.5 s.
      const done = Math.max(readAt, lastByteAt) + READ_TIMEOUT + 300;
      if (now() < done) await sleep(done - now());
      this._assertOpen();
      return result;
    } finally {
      this._setBusy(false);
    }
  }

  _assertOpen() {
    if (this.state === 'closed') throw new Error('Die Verbindung zur Uhr wurde unterbrochen');
  }

  refreshStatus() {
    return this.command(commands.status(), { expect: (l) => (l === STATUS_END ? this.status : undefined), label: 'Einstellungen lesen' });
  }

  async setColor(color) {
    await this.command(commands.color(color), { label: 'Farbe' });
  }

  async setMode(mode) {
    await this.command(commands.mode(mode), { label: 'Farbmodus' });
  }

  async setBrightness(s) {
    await this.command(commands.brightness(s), { label: 'Helligkeit' });
  }

  async setTimechange(on) {
    await this.command(commands.timechange(on), { label: 'Sommerzeit' });
  }

  async setLdr(n) {
    if (!(n >= 1 && n <= 9)) throw new Error('Der Teiler muss zwischen 1 und 9 liegen');
    await this.command(commands.ldr(n), { label: 'Lichtsensor' });
  }

  async factoryReset() {
    await this.command(commands.factoryReset(), { label: 'Werkseinstellungen' });
    return this.refreshStatus();
  }

  async selftest() {
    let ldr = null;
    const rtc = await this.command(commands.selftest(), {
      label: 'Selbsttest',
      timeout: 45000,
      expect: (l) => {
        if (/^(LDROK|LDRTH|LDRTL|PROBLEM)$/.test(l)) ldr = l;
        return /^RTC(OK|NOTSET)$/.test(l) ? l : undefined;
      },
    });
    return { ldr, rtc };
  }

  // Sends a '_' at `at`, but only while the clock is certainly still reading (previous byte at `last`).
  // Returns the send time, or null if it is too late (e.g. timers throttled in a background tab).
  async _tick(at, last) {
    if (now() < at) await sleep(at - now());
    const t = now();
    if (t - last > MAX_GAP) return null;
    await this._write('_', false);
    return t;
  }

  // Sets the clock to this computer's time, shifted by offsetMs (manual entry).
  //
  // The firmware reads the command until no byte has arrived for 2.5 s, waits 100 ms, echoes it and
  // immediately writes the RTC, whose current second starts at that moment. It adds 5 s to the sent time;
  // 6.1 carries that only into the minute, not the hour, so a second >= 55 is never sent.
  // Precision comes from a '_' sent after the command (both firmwares only parse the 12 digits after
  // the '@'): it is timed so that the RTC is written on a full second. If that moment is more than 2.2 s
  // after the read starts, further '_' bytes keep the read open. A byte arriving after the read has
  // ended would corrupt the next command, so no byte is ever sent after a longer gap.
  // retry: if the fine timing missed (the clock read later than expected, throttled timers), try once more.
  async setTime({ retry = true, ...opts } = {}) {
    let r = await this._setTimeOnce(opts);
    if (retry && !r.aligned && (r.errorMs === null || Math.abs(r.errorMs) > 100)) r = await this._setTimeOnce(opts);
    this._emit('timeset', r);
    return r;
  }

  async _setTimeOnce({ offsetMs = 0, timechange = this.status?.timechange ?? 1 }) {
    const fieldsFor = (ms) => (timechange === 0 ? berlinFields(ms) : cetFields(ms));
    if (!validYear(fieldsFor(Date.now() + offsetMs).y)) throw new Error('Die Uhr kann nur Jahre von 2016 bis 2099.');
    const shift = () => Date.now() - now() + offsetMs; // performance.now() -> target time (epoch ms)
    // The RTC is to be written on a full second of the target time, with the last byte sent around `aim`
    const plan = (aim) => {
      const show = Math.round((aim + this._writeLag + shift()) / 1000) * 1000; // shown by the clock from the RTC write on
      const fields = fieldsFor(show - 5000);
      return fields.s < 55 ? { fields, show, byteAt: show - shift() - this._writeLag } : null;
    };
    let p = null;
    let tickAt = null;
    const beforeSend = async () => {
      for (let tries = 0; tries < 8 && !p; tries++) {
        if (this.debugOutput) {
          // send right after a "Warte" line so the clock reads the command on its next pass
          p = plan((await this._nextWarte()) + this.loopPeriod + AIM_AFTER_READ);
        } else {
          // without debug output the clock reads some time in its next loop() pass: send AIM_BLIND ms before byteAt
          p = plan(now() + AIM_BLIND + 500 + tries * 1000);
          if (p) await sleep(p.byteAt - AIM_BLIND - now());
        }
      }
      if (!p) throw new Error('Die Uhrzeit konnte nicht gesendet werden');
    };
    const afterRead = async (readAt, sentAt) => {
      let last = readAt ?? sentAt; // the clock's read timeout runs from here (without debug output: at the earliest)
      if (p.byteAt < last + 100) return last; // the clock started reading later than planned
      while (p.byteAt - last > MAX_GAP) { // keep the read open
        const t = await this._tick(last + KEEP_GAP, last);
        if (t === null) return last;
        last = t;
      }
      tickAt = await this._tick(p.byteAt, last);
      return tickAt ?? last;
    };
    const echo = await this._enqueue(() => this._run(() => commands.time(p.fields), {
      beforeSend,
      afterRead,
      label: 'Uhrzeit',
      timeout: this.state === 'setup' ? 20000 : 12000,
      expect: (l, t) => (/^@\d{12}/.test(l) ? t : undefined),
    }));
    // Error (ms): the echo comes right before the RTC write, from then on the clock shows p.show. Positive = ahead.
    let errorMs = null;
    if (echo !== undefined) {
      const writtenAt = echo - ECHO_LAG;
      errorMs = Math.round(p.show - (writtenAt + shift()));
      // Adopt this clock's lag (crystal, USB) for the next time set
      if (tickAt !== null && Math.abs(writtenAt - tickAt - SET_LAG) < 100) this._writeLag = writtenAt - tickAt;
    }
    return { sent: p.fields, errorMs, aligned: tickAt !== null };
  }

  // Initial setup: set the time (the clock then initialises its EEPROM), then reconnect.
  async initialSetup() {
    const initDone = this.waitSeen(['eeprom'], now(), 60000, 'Bestätigung');
    initDone.catch(() => {});
    // no retry here: once the time is set, the clock leaves the LED test and finishes starting up
    let r = await this.setTime({ timechange: 1, retry: false });
    await initDone;
    await this._handshake();
    // The LED test reads at irregular intervals (1 or 2 s): if the fine timing missed, set it precisely now
    if (this.state === 'ready' && (r.errorMs === null || Math.abs(r.errorMs) > 100)) r = await this.setTime({ timechange: 1 });
    return r;
  }
}
