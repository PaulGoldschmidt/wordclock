// Verbindung zur Uhr ueber Web Serial: Handshake, Befehle und Auswertung der Ausgaben der Firmware.
// Funktioniert mit der Original-Firmware (6.1) und mit 6.2. Kein DOM-Zugriff, damit das Modul auch
// ausserhalb des Browsers (Tests) laeuft.
//
// Das Protokoll der Firmware (LightCore/communication_selfcheck.ino):
//  - Beim Oeffnen des Ports startet der Arduino neu. Kommt waehrend setup() ein Byte an, ueberspringt
//    die Uhr die Startanimation, begrenzt die Helligkeit und schreibt Debug-Ausgaben ("PC-Modus").
//  - Ein Befehl wird erst gelesen, wenn mindestens 13 Zeichen im Puffer liegen; danach liest die Uhr
//    bis 2,5 s lang nichts mehr kommt. Zwei Befehle duerfen also nie direkt hintereinander kommen.
//    Ein Zeichen, das waehrend des Lesens ankommt, verlaengert es: darauf beruht das genaue Stellen der Zeit.
//  - Im PC-Modus meldet jeder loop()-Durchlauf "Warte auf neue Befehle..." direkt vor dieser Pruefung.

const BAUD = 115200;
const HELLO = 'PC2ARDUINO';
const READ_TIMEOUT = 2500; // Serial.setTimeout() der Firmware fuer Befehle
const CMD_LEN = 13;

// Uhrzeit stellen (setTime)
const SET_LAG = READ_TIMEOUT + 100; // letztes Zeichen -> RTC geschrieben: Ende von readString(), dann delay(100)
const ECHO_LAG = 2; // die Echo-Zeile ist gut 1 ms nach dem Schreiben des RTC fertig gesendet, dazu USB
const MAX_GAP = READ_TIMEOUT - 300; // groesster sicherer Abstand zweier Zeichen, solange die Uhr liest
const KEEP_GAP = 1500; // Abstand der Fuellzeichen, die das Lesen offen halten
const AIM_AFTER_READ = 1300; // PC-Modus: geplanter Abstand Lesebeginn -> letztes Zeichen (verpasst: siehe retry)
const AIM_BLIND = 2600; // ohne Debug-Ausgabe: Befehl -> letztes Zeichen (gelesen wird innerhalb einer loop()-Runde)
const WARTE = 'Warte auf neue Befehle';
const STATUS_END = 'STATUS END';
const encoder = new TextEncoder();

// Startmeldungen, deren Zeitpunkt in _seen festgehalten wird
const MARKERS = [
  ['banner', /^--Lichtbild Startvorgang--/], ['springe', /^Springe Startup-Animation/], ['hi', /^HI$/],
  ['hello', /^Hello World - this is LightPicture/], ['eeprom', /^EEPROM ausgelesen/],
];

// Werte aus der Startausgabe, die 6.1 im Status nicht oder falsch meldet
const BOOT_KEYS = { Nummer: 'number', 'Tag max. Helligkeit': 'dayMax', 'Tag min. Helligkeit': 'dayMin', 'LDR-Teiler': 'ldr' };

export const now = () => performance.now();
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const pad = (cmd) => cmd.padEnd(CMD_LEN, '_');
export const two = (n) => String(n).padStart(2, '0');
const three = (n) => String(n).padStart(3, '0');

// ---- Zeit ----------------------------------------------------------------------------------------
// Die Uhr erwartet im RTC immer Winterzeit (MEZ = UTC+1) und stellt selbst auf Sommerzeit um.
// Ist die automatische Umstellung aus, bekommt sie die deutsche Ortszeit.
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

// Umkehrung von utcFields: Felder als UTC gelesen -> ms
export const fieldsToMs = (f) => Date.UTC(f.y, f.mo - 1, f.d, f.h, f.mi, f.s);

export const cetFields = (ms) => utcFields(ms + 3600e3);

// Deutsche Ortszeit (z.B. aus einem Eingabefeld) -> Zeitpunkt in ms
export function berlinToMs(y, mo, d, h, mi, s = 0) {
  const guess = fieldsToMs({ y, mo, d, h, mi, s });
  for (const off of [1, 2]) {
    const ms = guess - off * 3600e3;
    const f = berlinFields(ms);
    if (f.y === y && f.mo === mo && f.d === d && f.h === h && f.mi === mi) return ms;
  }
  return guess - 3600e3; // Uhrzeit faellt in die Luecke der Umstellung
}

// Jahre, die die Uhr kennt (RTC_Module.ino)
const validYear = (y) => y >= 2016 && y <= 2099;

// ---- Befehle ---------------------------------------------------------------------------------------
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

// ---- Verbindung --------------------------------------------------------------------------------------
export class LightCore extends EventTarget {
  constructor(port) {
    super();
    this.port = port;
    this.state = 'closed'; // closed | connecting | setup | ready | limited
    this.firmware = null; // '6.1' | '6.2' | ...
    this.legacy = false; // Original-Firmware 6.1: ohne Zusatzzeilen im Status, andere Nachtlogik
    this.boot = {}; // Werte aus der Startausgabe
    this.status = null; // Antwort auf '&'
    this.telemetry = {}; // Debug-Ausgabe jedes loop()-Durchlaufs
    this.rtcError = false;
    this.busy = false; // ein Befehl laeuft
    this.busyLabel = null;
    this._waiters = new Set();
    this._queue = Promise.resolve();
    this._warteTimes = [];
    this._statusLines = null;
    this._lastLoopLine = 0;
    this._lastWarte = 0;
    this._seen = {}; // Zeitpunkt der letzten Startmeldungen (Banner, PC-Modus, ...)
    this._writeLag = SET_LAG + 3; // letztes Zeichen gesendet -> RTC geschrieben, wird beim Stellen nachgemessen
  }

  // Uhrzeit der Uhr, zwischen den Debug-Zeilen hochgezaehlt (null, solange unbekannt oder ungueltig)
  get clock() {
    const c = this.telemetry.clock;
    if (!c || this.rtcError) return null;
    return utcFields(fieldsToMs(c) + (Date.now() - c.at));
  }

  // ---------- Ereignisse ----------
  _emit(type, detail) { this.dispatchEvent(new CustomEvent(type, { detail })); }
  _setState(s) {
    if (this.state === s) return;
    this.state = s;
    this._warteTimes = []; // Takt der Uhr je Zustand neu messen (LED-Test laeuft anders als loop())
    this._lastWarte = 0;
    this._emit('state', s);
  }
  _log(text, dir = 'info') { this._emit('log', { text, dir }); }

  // Wartet auf die erste Zeile (ab jetzt), fuer die test(line, t) etwas anderes als undefined liefert.
  waitFor(test, timeout, what = 'Antwort') {
    return new Promise((resolve, reject) => {
      const w = { test, resolve, reject };
      w.timer = setTimeout(() => { this._waiters.delete(w); reject(new Error(`Keine ${what} von der Uhr (Zeitüberschreitung)`)); }, timeout);
      this._waiters.add(w);
    });
  }

  // Wie waitFor, aber auch erfuellt, wenn eine der Meldungen seit `since` schon kam. Noetig, weil mehrere
  // Zeilen im selben USB-Paket ankommen und synchron verarbeitet werden.
  waitSeen(keys, since, timeout, what) {
    const hit = () => keys.find((k) => this._seen[k] >= since);
    const h = hit();
    return h ? Promise.resolve(h) : this.waitFor(() => hit(), timeout, what);
  }

  // Zeitpunkt der naechsten "Warte"-Zeile nach `after`: kurz danach prueft die Uhr auf neue Befehle
  _nextWarte(after = 0) {
    return this.waitFor((l, t) => (l.startsWith(WARTE) && t > after ? t : undefined), 9000, 'Reaktion');
  }

  _failWaiters(err) {
    for (const w of this._waiters) { clearTimeout(w.timer); w.reject(err); }
    this._waiters.clear();
  }

  // ---------- Port ----------
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

  // restart: die Uhr vor dem Trennen neu starten, damit sie ohne PC-Modus (volle Helligkeit) weiterlaeuft
  async close(restart = true) {
    if (this.state === 'closed' && !this.port.readable) return;
    this._closing = true;
    if (restart && this.port.writable) {
      try { await this._resetBoard(); } catch { /* Port evtl. schon weg */ }
    }
    try { await this._reader?.cancel(); } catch { /* egal */ }
    await this._readDone;
    try { await this.port.close(); } catch { /* egal */ }
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
            if (buf.length > 4096) buf = buf.slice(-1024); // Datenmuell ohne Zeilenende
          }
        } catch (e) {
          if (!this._closing) this._log(`Lesefehler: ${e.message}`, 'error');
        } finally {
          this._reader.releaseLock();
          this._reader = null;
        }
      }
      if (!this._closing) { // Kabel gezogen o.ae.
        this._failWaiters(new Error('Die Verbindung zur Uhr wurde unterbrochen'));
        this._setState('closed');
        this._emit('lost');
      }
    })();
  }

  // Reset ueber DTR (wie die Arduino-IDE): Flanke "aus -> an" loest ueber den Kondensator den Reset aus.
  async _resetBoard() {
    await this.port.setSignals({ dataTerminalReady: false, requestToSend: false });
    await sleep(120);
    await this.port.setSignals({ dataTerminalReady: true, requestToSend: true });
  }

  // ---------- Handshake ----------
  async _handshake(retried = false) {
    this._setState('connecting');
    this.boot = {};
    this.status = null;
    this.telemetry = {};
    this._lastLoopLine = 0;
    this.rtcError = false;
    const t0 = now();
    await this._resetBoard();

    // Bis zum Startbanner immer wieder "Hallo" senden: was waehrend des Bootloaders ankommt, geht verloren.
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
    if (result === 'hello') { // neue Uhr: EEPROM leer, wartet auf die Uhrzeit
      this._setState('setup');
      // Im LED-Test liest die Uhr erst ab 13 Zeichen: Reste der "Hallo"-Bytes bleiben sonst im Puffer
      // und verderben den ersten Befehl. Ein Fuellbefehl schiebt sie als ungueltigen Befehl hinaus.
      await this.command('', { label: 'Verbindung vorbereiten' });
      return undefined;
    }
    if (result !== 'springe') {
      // Die Uhr hat das "Hallo" verpasst und spielt ihre Startanimation: einmal neu versuchen.
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
    this._lastLoopLine = now(); // Frist fuer die erste Debug-Zeile (RTC-Pruefung) beginnt jetzt
    this._setState('ready');
    await this.refreshStatus();
    return undefined;
  }

  // Kein Startbanner: die Uhr wurde beim Verbinden nicht neu gestartet (z.B. ohne DTR-Leitung).
  async _probeWithoutReset() {
    await sleep(READ_TIMEOUT + 800); // die Uhr hat die "Hallo"-Bytes evtl. als Befehl gelesen
    this._setState('limited');
    try {
      await this.refreshStatus();
    } catch {
      throw new Error('Die Uhr antwortet nicht. Ist der richtige Anschluss gewählt und die Uhr mit Strom versorgt?');
    }
  }

  // ---------- Zeilen der Firmware auswerten ----------
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
      // Abstand zweier Durchlaeufe merken, aber nicht, wenn dazwischen ein Befehl gelesen wurde (>= 2,5 s)
      const gap = t - this._lastWarte;
      if (this._lastWarte && gap > 900 && gap < 2300) this._warteTimes = [...this._warteTimes.slice(-7), gap];
      this._lastWarte = t;
      // 6.2 zeigt bei RTC-Fehler keine Debug-Ausgabe: "Warte" ohne "Loop durchlaufen" = RTC-Fehler
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
    this.legacy = extra.VERSION === undefined; // 6.1 kennt die Zusatzzeilen noch nicht
    this.firmware = extra.VERSION || '6.1';
    this.status = {
      dayMax, dayMin, nightMax, nightMin, nightBegin, nightEnd, color: [r, g, b], mode, timechange,
      startups, runtime,
      ldr: extra.LDR !== undefined ? Number(extra.LDR) : this.boot.ldr,
      number: extra.NR !== undefined ? Number(extra.NR) : this.boot.number,
    };
    // 6.1 meldet im PC-Modus die voruebergehend begrenzte Helligkeit (39/40) statt der gespeicherten.
    // Die Tag-Werte stehen in der Startausgabe, die Nacht-Werte sind nicht auslesbar (null).
    if (this.legacy && this.state !== 'limited') {
      this.status.dayMax = this.boot.dayMax ?? null;
      this.status.dayMin = this.boot.dayMin ?? null;
      this.status.nightMax = null;
      this.status.nightMin = null;
    }
    this._emit('status', this.status);
  }

  // ---------- Befehle ----------
  get loopPeriod() {
    const w = this._warteTimes;
    if (!w.length) return this.legacy ? 1800 : 1550;
    return w.slice().sort((a, b) => a - b)[Math.floor(w.length / 2)];
  }

  get debugOutput() { return this.state === 'ready' || this.state === 'setup'; }

  // Reiht einen Befehl ein. Er wird erst gesendet, wenn die Uhr den vorigen fertig gelesen hat.
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

  // beforeSend(): vor dem Senden (z.B. auf den richtigen Moment warten)
  // afterRead(readAt, sentAt): nach dem Senden, waehrend die Uhr liest; liefert den Zeitpunkt des
  // letzten gesendeten Zeichens. readAt = beobachteter Lesebeginn oder null (ohne Debug-Ausgabe).
  async _run(cmd, { expect, timeout, label, beforeSend, afterRead }) {
    if (this.state === 'closed' || this.state === 'connecting') throw new Error('Nicht verbunden');
    this._setBusy(true, label);
    try {
      if (beforeSend) await beforeSend();
      const text = typeof cmd === 'function' ? cmd() : cmd;
      const sentAt = now();
      // Die erste "Warte"-Zeile, die sicher nach dem Eintreffen unserer Bytes geschrieben wurde,
      // markiert den Moment, in dem die Uhr den Befehl liest.
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
      // Erst wenn die Uhr 2,5 s nichts mehr empfangen hat, ist der Befehl wirklich abgeschlossen.
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

  // Sendet um `at` ein '_', aber nur, solange die Uhr sicher noch liest (letztes Zeichen bei `last`).
  // Liefert den Sendezeitpunkt oder null, wenn es dafuer zu spaet ist (z.B. gedrosselte Timer im Hintergrund).
  async _tick(at, last) {
    if (now() < at) await sleep(at - now());
    const t = now();
    if (t - last > MAX_GAP) return null;
    await this._write('_', false);
    return t;
  }

  // Uhrzeit stellen. offsetMs verschiebt die Zeit (manuelle Eingabe), sonst gilt die Uhrzeit dieses Computers.
  //
  // Die Firmware liest den Befehl, bis 2,5 s kein Zeichen mehr kommt, wartet 100 ms, schickt ihn als Echo
  // zurueck und schreibt sofort das RTC, das dabei seine Sekunde neu beginnt. Sie rechnet 5 s hinzu; 6.1
  // kann dabei keinen Stundenuebertrag, deshalb wird nie eine Sekunde >= 55 gesendet.
  // Genau wird es durch ein '_' nach dem Befehl (beide Firmwares lesen nur die Stellen 1-12): es kommt
  // so an, dass das RTC auf eine volle Sekunde geschrieben wird. Liegt dieser Moment weiter als 2,2 s nach
  // dem Lesebeginn, halten weitere '_' das Lesen offen. Ein Zeichen nach dem Ende des Lesens wuerde den
  // naechsten Befehl verderben, deshalb wird keines mit groesserem Abstand gesendet.
  // retry: ging die Feinabstimmung daneben (Uhr las spaeter als erwartet, gedrosselte Timer), einmal wiederholen
  async setTime({ retry = true, ...opts } = {}) {
    let r = await this._setTimeOnce(opts);
    if (retry && !r.aligned && (r.errorMs === null || Math.abs(r.errorMs) > 100)) r = await this._setTimeOnce(opts);
    this._emit('timeset', r);
    return r;
  }

  async _setTimeOnce({ offsetMs = 0, timechange = this.status?.timechange ?? 1 }) {
    const fieldsFor = (ms) => (timechange === 0 ? berlinFields(ms) : cetFields(ms));
    if (!validYear(fieldsFor(Date.now() + offsetMs).y)) throw new Error('Die Uhr kann nur Jahre von 2016 bis 2099.');
    const shift = () => Date.now() - now() + offsetMs; // performance.now() -> Zielzeit
    // Das RTC soll auf einer vollen Sekunde der Zielzeit geschrieben werden, das letzte Zeichen etwa bei `aim`
    const plan = (aim) => {
      const show = Math.round((aim + this._writeLag + shift()) / 1000) * 1000; // zeigt die Uhr ab dem Schreiben
      const fields = fieldsFor(show - 5000);
      return fields.s < 55 ? { fields, show, byteAt: show - shift() - this._writeLag } : null;
    };
    let p = null;
    let tickAt = null;
    const beforeSend = async () => {
      for (let tries = 0; tries < 8 && !p; tries++) {
        if (this.debugOutput) {
          // direkt nach einer "Warte"-Zeile senden: dann liest die Uhr den Befehl beim naechsten Durchlauf
          p = plan((await this._nextWarte()) + this.loopPeriod + AIM_AFTER_READ);
        } else {
          // ohne Debug-Ausgabe liest die Uhr irgendwann in der naechsten loop()-Runde: Befehl AIM_BLIND vorher
          p = plan(now() + AIM_BLIND + 500 + tries * 1000);
          if (p) await sleep(p.byteAt - AIM_BLIND - now());
        }
      }
      if (!p) throw new Error('Die Uhrzeit konnte nicht gesendet werden');
    };
    const afterRead = async (readAt, sentAt) => {
      let last = readAt ?? sentAt; // ab hier laeuft der Timeout der Uhr (ohne Debug-Ausgabe: fruehestens)
      if (p.byteAt < last + 100) return last; // die Uhr hat spaeter gelesen als geplant
      while (p.byteAt - last > MAX_GAP) { // Lesen offen halten
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
    // Abweichung: Das Echo kommt direkt vor dem Schreiben. Die Uhr zeigt ab da p.show. Positiv = Uhr geht vor.
    let errorMs = null;
    if (echo !== undefined) {
      const writtenAt = echo - ECHO_LAG;
      errorMs = Math.round(p.show - (writtenAt + shift()));
      // Verzoegerung dieser Uhr (Quarz, USB) fuer das naechste Stellen uebernehmen
      if (tickAt !== null && Math.abs(writtenAt - tickAt - SET_LAG) < 100) this._writeLag = writtenAt - tickAt;
    }
    return { sent: p.fields, errorMs, aligned: tickAt !== null };
  }

  // Ersteinrichtung: Zeit setzen (die Uhr beschreibt dann ihr EEPROM) und danach neu verbinden.
  async initialSetup() {
    const initDone = this.waitSeen(['eeprom'], now(), 60000, 'Bestätigung');
    initDone.catch(() => {});
    // kein Wiederholen hier: nach dem Stellen startet die Uhr neu
    let r = await this.setTime({ timechange: 1, retry: false });
    await initDone;
    await this._handshake();
    // Im LED-Test liest die Uhr unregelmaessig (1 oder 2 s): ging die Feinabstimmung daneben, jetzt genau stellen
    if (this.state === 'ready' && (r.errorMs === null || Math.abs(r.errorMs) > 100)) r = await this.setTime({ timechange: 1 });
    return r;
  }
}
