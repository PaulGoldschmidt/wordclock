import { LightCore, berlinFields, berlinToMs, cetFields, commands, fieldsToMs, now, two } from './lightcore.js';
import { FACTORY, WordClockModel, fillFrame, isNight, logoFrame, phrase } from './wordclock.js';
import { Plate, miniPlate, rgb } from './plate.js';

const $ = (id) => document.getElementById(id);
const plate = new Plate($('plate'));

// USB-Seriell-Wandler der ueblichen Arduino Nano / Pro Mini (Original, CH340, FTDI, CP210x, Prolific)
const USB_FILTERS = [0x2341, 0x2a03, 0x1a86, 0x0403, 0x10c4, 0x067b].map((usbVendorId) => ({ usbVendorId }));

const SWATCHES = [
  ['Eisblau (Werkseinstellung)', FACTORY.color],
  ['Warmweiß', [255, 147, 41]],
  ['Weiß', [255, 255, 255]],
  ['Rot', [255, 0, 0]],
  ['Orange', [255, 72, 0]],
  ['Gelb', [255, 190, 0]],
  ['Grün', [0, 255, 50]],
  ['Türkis', [0, 255, 170]],
  ['Blau', [0, 40, 255]],
  ['Violett', [140, 0, 255]],
  ['Pink', [255, 0, 130]],
];

const MODE_NOTES = [
  'Alle Buchstaben in einer Farbe.',
  'Die Farbe wandert langsam durch den Regenbogen: jede Minute ein kleiner Schritt, einmal ganz herum in knapp 13 Stunden.',
  'Jeder Buchstabe eine Farbe weiter, wie ein Farbverlauf durch den Satz.',
  'Ein Regenbogen von oben nach unten, eine Farbe pro Zeile.',
];

// Einstellungen der Uhr, wie sie in ui.draft und ui.known stehen
const BRIGHT_KEYS = ['dayMin', 'dayMax', 'nightMin', 'nightMax', 'nightBegin', 'nightEnd'];
const SLIDERS = ['dayMin', 'dayMax', 'nightMin', 'nightMax', 'ldr'];
const SETTINGS = [...BRIGHT_KEYS, 'ldr', 'timechange', 'mode'];

const clone = (s) => ({ ...s, color: [...s.color] });
const hm = (h, mi) => `${two(h)}:${two(mi)}`;
const hms = (f) => `${hm(f.h, f.mi)}:${two(f.s)}`;

const ui = {
  lc: null,
  state: 'closed',
  draft: clone(FACTORY), // was in der Oberflaeche eingestellt ist
  known: null, // was die Uhr gespeichert hat; einzelne Werte null = nicht auslesbar
  scrubMin: null, // Uhrzeit der Vorschau in Minuten, null = jetzt
  room: 'hell',
  overlay: null, // Statusbild der Uhr: { kind, start }
  timeMsg: null, // { text, cls, until }
};

const usable = () => Boolean(ui.lc) && (ui.state === 'ready' || ui.state === 'limited');
const busy = () => Boolean(ui.lc?.busy);
const nightAt = (h) => isNight(h, ui.draft.nightBegin, ui.draft.nightEnd, ui.lc?.legacy);
// Uhrzeit, die die Uhr zeigen sollte: mit Sommerzeit deutsche Ortszeit, sonst immer MEZ
const wallFields = () => (ui.draft.timechange ? berlinFields : cetFields)(Date.now());
const clockDst = () => ui.known?.timechange ?? ui.draft.timechange;

let model = new WordClockModel(ui.draft.color, ui.draft.mode);
let frameCache = { key: '', leds: null };
let lastPlateKey = '';

// ---------- Vorschau ----------
// Farbe oder Modus geaendert
function modelChanged() {
  model = new WordClockModel(ui.draft.color, ui.draft.mode);
  frameCache.key = '';
  updateMinis();
  syncControls();
}

function displayTime() {
  if (ui.scrubMin !== null) return { h: Math.floor(ui.scrubMin / 60), mi: ui.scrubMin % 60, source: 'scrub' };
  const c = ui.lc?.clock;
  return c ? { ...c, source: 'clock' } : { ...wallFields(), source: 'computer' };
}

function previewBrightness(h) {
  const d = ui.draft;
  const night = nightAt(h);
  const hell = ui.room === 'hell';
  return { night, b: night ? (hell ? d.nightMax : d.nightMin) : (hell ? d.dayMax : d.dayMin) };
}

const visual = (b) => (b <= 0 ? 0 : 0.18 + 0.82 * Math.sqrt(Math.min(255, b) / 255));

// Statusbilder, die die echte Uhr gerade zeigt (effects.ino), mit Bildunterschrift.
// smooth: weich ueberblenden; sonst schaltet die Vorschau hart um wie die Uhr.
function overlayFrame(t) {
  const o = ui.overlay;
  if (o) {
    const dt = t - o.start;
    switch (o.kind) {
      case 'connecting':
        return { leds: logoFrame(), level: 0.55 + 0.45 * Math.sin(dt / 260), smooth: true, caption: 'Verbinde mit der Uhr …' };
      case 'reading':
        return { leds: logoFrame([0, 0, 255]), level: 1, caption: `Die Uhr empfängt: <b>${ui.lc?.busyLabel || 'Befehl'}</b>` };
      case 'selftest': {
        const caption = 'Selbsttest läuft: alle LEDs weiß, dann zählt die Uhr einmal durch';
        if (dt < 2600 || dt >= 30100) return { leds: logoFrame([0, 0, 255]), level: 1, caption };
        if (dt < 17600) return { leds: fillFrame([255, 255, 255]), level: visual(100), caption };
        return { leds: fillFrame([255, 255, 255], Math.floor((dt - 17600) / 100) + 1), level: visual(50), caption };
      }
      case 'init':
        return { leds: logoFrame([255, 255, 0]), level: Math.floor(dt / 500) % 2 ? 0.15 : 1, caption: 'Die Uhr richtet sich ein …' };
      default: break;
    }
  }
  if (ui.state === 'setup') { // LED-Test einer neuen Uhr: rot, gruen, blau, weiss, Logo
    const step = Math.floor(t / 1000) % 5;
    const caption = 'Neue Uhr: sie testet ihre LEDs und wartet auf die Uhrzeit';
    return step < 4
      ? { leds: fillFrame([[255, 0, 0], [0, 128, 0], [0, 0, 255], [255, 255, 255]][step]), level: visual(20), caption }
      : { leds: logoFrame(), level: 1, caption };
  }
  if (ui.state === 'ready' && ui.lc?.rtcError && !busy()) {
    return { leds: logoFrame([0, 255, 0]), level: Math.floor(t / 1000) % 2 ? 0 : 1, caption: 'Die Uhr kennt die Uhrzeit nicht: sie blinkt grün' };
  }
  return null;
}

function render() {
  const t = now();
  const tm = displayTime();
  const o = overlayFrame(t);
  let leds;
  let level;
  if (o) {
    ({ leds, level } = o);
  } else {
    const key = `${tm.h}:${tm.mi}`;
    if (key !== frameCache.key) frameCache = { key, leds: model.render(tm.h, tm.mi) };
    leds = frameCache.leds;
    level = visual(previewBrightness(tm.h).b);
  }
  const svg = $('plate');
  const plateKey = `${level.toFixed(2)}|${leds.map((v) => (v ? v.join(',') : '')).join(';')}`;
  if (plateKey !== lastPlateKey) {
    lastPlateKey = plateKey;
    svg.classList.toggle('is-instant', Boolean(o) && !o.smooth);
    const glow = plate.set(leds, level);
    const wall = $('wall');
    if (glow) {
      wall.style.setProperty('--led-soft', `rgba(${glow.color.join(',')}, 0.24)`);
      wall.style.setProperty('--glow-amount', String(0.25 + 0.75 * glow.amount));
    } else {
      wall.style.setProperty('--glow-amount', '0');
    }
  }
  const label = o ? 'Statusanzeige der Uhr' : phrase(tm.h, tm.mi);
  if (svg.getAttribute('aria-label') !== label) svg.setAttribute('aria-label', label);
  renderCaption(tm, o);
  showScrub(tm.h * 60 + tm.mi);
  renderTimes();
  renderHours(tm);
  renderLdr();
}

function renderLdr() {
  const tel = ui.state === 'ready' ? ui.lc?.telemetry : null;
  const box = $('ldrLive');
  box.hidden = !tel || tel.ldr === undefined;
  if (!box.hidden) {
    const text = `Sensor jetzt: ${tel.ldr}, Helligkeit: ${tel.brightness}`;
    if (box.textContent !== text) box.textContent = text;
  }
}

function renderCaption(tm, o) {
  let text = o?.caption;
  if (!text) {
    const { night, b } = previewBrightness(tm.h);
    const src = tm.source === 'clock' ? 'auf deiner Uhr' : 'in der Vorschau';
    const p = phrase(tm.h, tm.mi).replace(/ \(\+\d\)$/, '');
    text = `<b>${p}</b> ${src}, ${night ? 'nachts' : 'tagsüber'} mit Helligkeit ${b}`;
  }
  if ($('caption').innerHTML !== text) $('caption').innerHTML = text;
}

function showScrub(min) {
  const s = $('scrub');
  if (s.value !== String(min)) {
    s.value = String(min);
    setFill(s);
  }
  $('scrubLabel').textContent = hm(Math.floor(min / 60), min % 60);
  $('liveBtn').setAttribute('aria-pressed', String(ui.scrubMin === null));
}

function renderTimes() {
  const wall = wallFields();
  $('wallTime').textContent = hms(wall);
  const c = ui.lc?.clock;
  $('clockTime').textContent = c ? hms(c) : '--:--:--';
  let msg = { text: 'Verbinde die Uhr, um ihre Uhrzeit zu sehen.', cls: '' };
  if (ui.timeMsg && now() < ui.timeMsg.until) msg = ui.timeMsg;
  else if (ui.state === 'setup') msg = { text: 'Wird beim Einrichten gestellt.', cls: '' };
  else if (ui.state === 'limited') msg = { text: 'Die Uhr meldet im eingeschränkten Modus keine Uhrzeit. Stellen geht trotzdem.', cls: '' };
  else if (ui.lc?.rtcError) msg = { text: 'Die Uhr kennt die Uhrzeit nicht (Knopfzelle leer?). Bitte Uhrzeit übernehmen.', cls: 'warn' };
  else if (c) {
    let diff = (fieldsToMs(c) - fieldsToMs(wall)) / 1000;
    if (Math.abs(diff) < 2) msg = { text: 'Die Uhr geht genau.', cls: 'good' };
    else {
      const dir = diff > 0 ? 'vor' : 'nach';
      diff = Math.abs(diff);
      const parts = [];
      if (diff >= 86400) parts.push(`${Math.floor(diff / 86400)} Tage`);
      if (diff >= 3600) parts.push(`${Math.floor(diff / 3600) % 24} h`);
      if (diff >= 60) parts.push(`${Math.floor(diff / 60) % 60} min`);
      if (diff < 3600) parts.push(`${Math.round(diff % 60)} s`);
      msg = { text: `Die Uhr geht ${parts.join(' ')} ${dir}.`, cls: 'warn' };
    }
  } else if (ui.state === 'ready') msg = { text: 'Warte auf die Uhrzeit der Uhr …', cls: '' };
  const n = $('timeNote');
  if (n.textContent !== msg.text) n.textContent = msg.text;
  n.className = `note ${msg.cls}`;
}

function renderHours(tm) {
  const box = $('hours');
  if (!box.children.length) {
    for (let h = 0; h < 24; h++) {
      const s = document.createElement('span');
      s.textContent = h;
      box.appendChild(s);
    }
  }
  [...box.children].forEach((s, h) => {
    s.classList.toggle('night', nightAt(h));
    s.classList.toggle('now', h === tm.h);
  });
}

function updateMinis() {
  document.querySelectorAll('.mode').forEach((btn) => {
    const mode = Number(btn.dataset.mode);
    const svg = btn.querySelector('svg');
    // Die Farbmodi beginnen immer bei Rot: nur "Eine Farbe" haengt von der gewaehlten Farbe ab
    if (mode === 0 || !svg.childNodes.length) {
      miniPlate(svg, new WordClockModel(ui.draft.color, mode).render(10, 23)); // ES IST ZWANZIG NACH ZEHN + 3 Punkte
    }
    btn.setAttribute('aria-checked', String(mode === ui.draft.mode));
  });
  // Akzentfarbe der Seite = Farbe der Uhr (fuer dunkle Farben aufgehellt)
  const c = ui.draft.mode === 0 ? ui.draft.color : [255, 120, 60];
  const max = Math.max(...c, 1);
  document.documentElement.style.setProperty('--led', rgb(c.map((v) => Math.round((v * 255) / max))));
}

// ---------- Bedienelemente ----------
function setFill(input) {
  const pct = ((input.value - input.min) / (input.max - input.min)) * 100;
  input.style.setProperty('--fill', `${pct}%`);
}

function syncControls() {
  const d = ui.draft;
  for (const id of SLIDERS) {
    $(id).value = d[id];
    $(id).nextElementSibling.textContent = d[id];
    setFill($(id));
  }
  $('nightBegin').value = d.nightBegin;
  $('nightEnd').value = d.nightEnd;
  $('dst').checked = Boolean(d.timechange);
  $('colorPick').value = `#${d.color.map((v) => two(v.toString(16))).join('')}`;
  $('rgbLabel').textContent = d.color.join(' / ');
  $('colors').hidden = d.mode !== 0;
  $('modeNote').textContent = MODE_NOTES[d.mode];
  document.querySelectorAll('.swatch').forEach((s) => s.setAttribute('aria-checked', String(s.dataset.c === d.color.join())));
  updateDirty();
  updateBrightNote();
}

function updateDirty() {
  const d = ui.draft;
  const k = ui.known;
  const differs = (keys) => !k || keys.some((x) => d[x] !== k[x]);
  const dirty = {
    applyColor: differs(['mode']) || (d.mode === 0 && d.color.join() !== k.color.join()),
    applyBright: differs(BRIGHT_KEYS),
    applyLdr: differs(['ldr']),
  };
  for (const [id, v] of Object.entries(dirty)) $(id).classList.toggle('is-dirty', usable() && v);
}

function updateBrightNote() {
  const d = ui.draft;
  const notes = [];
  if (ui.lc?.legacy && d.nightBegin <= d.nightEnd) {
    notes.push('Mit der Original-Firmware 6.1 klappt nur ein Nachtzeitraum über Mitternacht (Beginn später als Ende).');
  }
  if (ui.known?.nightMax === null) {
    notes.push('Diese Firmware verrät die gespeicherte Nacht-Helligkeit nicht. Angezeigt sind die Werkseinstellungen.');
  }
  if (d.dayMax === 0 || d.nightMax === 0) notes.push('Mit Maximum 0 bleibt die Uhr dunkel.');
  $('brightNote').hidden = !notes.length;
  $('brightNote').textContent = notes.join(' ');
}

function buildControls() {
  const sw = $('swatches');
  for (const [name, c] of SWATCHES) {
    const b = document.createElement('button');
    b.className = 'swatch';
    b.type = 'button';
    b.setAttribute('role', 'radio');
    b.title = name;
    b.setAttribute('aria-label', name);
    b.dataset.c = c.join();
    b.style.setProperty('--c', rgb(c));
    b.addEventListener('click', () => { ui.draft.color = [...c]; modelChanged(); });
    sw.appendChild(b);
  }
  for (const sel of [$('nightBegin'), $('nightEnd')]) {
    for (let h = 0; h < 24; h++) sel.add(new Option(String(h), String(h)));
  }
  $('colorPick').addEventListener('input', (e) => {
    const v = e.target.value;
    ui.draft.color = [1, 3, 5].map((i) => parseInt(v.slice(i, i + 2), 16));
    modelChanged();
  });
  document.querySelectorAll('.mode').forEach((btn) => btn.addEventListener('click', () => {
    ui.draft.mode = Number(btn.dataset.mode);
    modelChanged();
  }));
  // Min/Max-Paare: das Minimum darf nicht ueber dem Maximum liegen
  const pairs = { dayMin: 'dayMax', dayMax: 'dayMin', nightMin: 'nightMax', nightMax: 'nightMin' };
  for (const id of SLIDERS) {
    $(id).addEventListener('input', (e) => {
      const v = Number(e.target.value);
      ui.draft[id] = v;
      const other = pairs[id];
      if (other) {
        if (id.endsWith('Min') && ui.draft[other] < v) ui.draft[other] = v;
        if (id.endsWith('Max') && ui.draft[other] > v) ui.draft[other] = v;
        // Vorschau zeigt den Wert, der gerade bewegt wird: Min = dunkler Raum, Nacht = eine Nachtstunde
        ui.room = id.endsWith('Min') ? 'dunkel' : 'hell';
        const wantNight = id.startsWith('night');
        if (wantNight !== nightAt(displayTime().h)) {
          const h = [...Array(24).keys()].map((x) => (x + 12) % 24).find((x) => nightAt(x) === wantNight);
          if (h !== undefined) previewAt(h * 60);
        }
      }
      syncRoom();
      syncControls();
    });
  }
  for (const id of ['nightBegin', 'nightEnd']) {
    $(id).addEventListener('change', (e) => { ui.draft[id] = Number(e.target.value); syncControls(); });
  }
  $('scrub').addEventListener('input', (e) => previewAt(Number(e.target.value)));
  $('liveBtn').addEventListener('click', () => previewAt(null));
  document.querySelectorAll('.seg button').forEach((b) => b.addEventListener('click', () => { ui.room = b.dataset.room; syncRoom(); }));

  // Abschnitte heben ihre Woerter auf der Platte hervor (UHR, TAG, NACHT)
  document.querySelectorAll('[data-words]').forEach((elm) => {
    const cells = elm.dataset.words.split(' ').map((x) => x.split('/').map(Number));
    const on = () => plate.hint(cells);
    const off = () => plate.hint([]);
    elm.addEventListener('pointerenter', on);
    elm.addEventListener('pointerleave', off);
    elm.addEventListener('focusin', on);
    elm.addEventListener('focusout', off);
  });
}

// Vorschau auf eine Uhrzeit (Minuten) stellen, null = jetzt
function previewAt(minutes) {
  ui.scrubMin = minutes;
  render();
}

function syncRoom() {
  document.querySelectorAll('.seg button').forEach((b) => b.setAttribute('aria-pressed', String(b.dataset.room === ui.room)));
}

// ---------- Verbindung ----------
let bannerId = 0;
function banner(html, kind = 'info', actions = []) {
  const b = $('banner');
  b.innerHTML = html;
  b.className = `banner ${kind === 'error' ? 'is-error' : ''}`;
  for (const [label, fn] of actions) {
    const btn = document.createElement('button');
    btn.className = 'btn';
    btn.style.marginLeft = '12px';
    btn.textContent = label;
    btn.addEventListener('click', fn);
    b.appendChild(btn);
  }
  b.hidden = false;
  return ++bannerId;
}

// id: nur diesen Hinweis ausblenden, falls er noch angezeigt wird
function hideBanner(id = bannerId) {
  if (id === bannerId) $('banner').hidden = true;
}

function setState(s) {
  ui.state = s;
  if (s === 'connecting') ui.overlay = { kind: 'connecting', start: now() };
  else if (ui.overlay?.kind === 'connecting') ui.overlay = null;
  if (s === 'closed') {
    ui.known = null;
    ui.overlay = null;
  }
  $('cardSetup').hidden = s !== 'setup';
  renderConn();
  updateButtons();
  updateDirty();
}

function renderConn() {
  const s = ui.state;
  const body = document.body.classList;
  body.toggle('is-connected', s === 'ready' || s === 'limited' || s === 'setup');
  body.toggle('is-connecting', s === 'connecting');
  const nr = ui.lc?.status?.number;
  $('connStatus').textContent = {
    closed: 'Nicht verbunden',
    connecting: 'Verbinde …',
    setup: 'Neue Uhr',
    ready: nr !== undefined ? `Verbunden mit Uhr Nr. ${nr}` : 'Verbunden',
    limited: 'Verbunden (eingeschränkt)',
  }[s];
  $('connectBtn').textContent = s === 'closed' ? 'Verbinden' : 'Trennen';
  $('connectBtn').classList.toggle('btn-primary', s === 'closed');
}

function updateButtons() {
  document.querySelectorAll('.needs-conn').forEach((b) => { b.disabled = !usable() || busy(); });
  $('setupBtn').disabled = ui.state !== 'setup' || busy();
  $('connectBtn').disabled = ui.state === 'connecting';
}

function loadFromClock(s) {
  const d = clone(ui.draft);
  for (const k of SETTINGS) d[k] = s[k] ?? FACTORY[k]; // nicht auslesbar (6.1: Nacht-Helligkeit): Werkseinstellung zeigen
  if (s.color) d.color = [...s.color]; // null: 6.1 in einem Farbwechsel-Modus, die feste Farbe ist nicht auslesbar
  ui.known = clone(d);
  for (const k of SETTINGS) ui.known[k] = s[k] ?? null; // unbekannt: "Uebernehmen" bleibt markiert
  ui.draft = d;
  $('infoNr').textContent = s.number ?? '–';
  $('infoFw').textContent = ui.lc.legacy ? '6.1 (Original)' : ui.lc.firmware;
  $('infoStarts').textContent = s.startups;
  const hours = s.runtime * 12;
  $('infoRuntime').textContent = hours >= 48 ? `${Math.round(hours / 24).toLocaleString('de-DE')} Tage` : `${hours} h`;
  modelChanged();
  renderConn(); // Uhrnummer im Kopf
}

// Nach erfolgreichem Senden: die Uhr hat jetzt diese Werte aus dem Entwurf
function markSent(keys) {
  if (ui.known) for (const k of keys) ui.known[k] = structuredClone(ui.draft[k]);
  updateDirty();
  updateBrightNote();
}

async function connect(port) {
  hideBanner();
  const lc = new LightCore(port);
  ui.lc = lc;
  lc.addEventListener('state', (e) => {
    if (e.detail === 'closed' && ui.lc === lc) ui.lc = null;
    setState(e.detail);
  });
  lc.addEventListener('status', (e) => loadFromClock(e.detail));
  lc.addEventListener('busy', () => {
    if (!lc.busy && (ui.overlay?.kind === 'reading' || ui.overlay?.kind === 'selftest')) ui.overlay = null;
    updateButtons();
  });
  lc.addEventListener('reading', (e) => {
    ui.overlay = { kind: e.detail.cmd === commands.selftest() ? 'selftest' : 'reading', start: e.detail.at };
  });
  lc.addEventListener('log', (e) => logLine(e.detail));
  lc.addEventListener('lost', () => {
    banner('Die Verbindung zur Uhr wurde unterbrochen. Ist das USB-Kabel noch eingesteckt?', 'error');
  });
  lc.addEventListener('timeset', (e) => {
    const err = e.detail.errorMs === null ? null : Math.abs(e.detail.errorMs);
    let text = 'Uhrzeit gesendet.';
    if (err !== null) text = `Gestellt, Abweichung ${err < 1000 ? `${err} ms` : `${(err / 1000).toLocaleString('de-DE', { maximumFractionDigits: 1 })} s`}.`;
    ui.timeMsg = {
      text,
      cls: 'good',
      until: now() + 8000,
    };
  });
  try {
    await lc.open();
    if (lc.state === 'ready') {
      const id = banner('Verbunden. Solange die Uhr am Computer hängt, leuchtet sie gedimmt. Beim Trennen startet sie neu und läuft normal weiter.');
      setTimeout(() => hideBanner(id), 9000);
    } else if (lc.state === 'limited') {
      banner('Verbunden, aber die Uhr hat beim Start den Computer nicht erkannt. Einstellen geht, nur ihre Uhrzeit wird nicht angezeigt.');
    }
  } catch (e) {
    ui.lc = null;
    setState('closed');
    let msg = e.message;
    if (e.name === 'InvalidStateError' || e.name === 'NetworkError') {
      msg = 'Der Anschluss ist belegt. Ist noch der serielle Monitor der Arduino-IDE oder ein anderes Programm offen?';
    }
    banner(msg, 'error');
  }
}

async function disconnect() {
  const lc = ui.lc;
  if (!lc) return;
  ui.lc = null;
  await lc.close().catch(() => {});
  setState('closed');
}

async function chooseAndConnect(all = false) {
  if (!('serial' in navigator)) return;
  let port;
  try {
    const granted = all ? [] : await navigator.serial.getPorts();
    port = granted.length === 1 ? granted[0] : await navigator.serial.requestPort(all ? {} : { filters: USB_FILTERS });
  } catch (e) {
    if (e.name === 'NotFoundError') {
      if (!all) {
        banner('Keine Uhr ausgewählt. Taucht sie nicht in der Liste auf?', 'info', [['Alle Geräte anzeigen', () => chooseAndConnect(true)]]);
      }
      return;
    }
    banner(e.message, 'error');
    return;
  }
  connect(port);
}

// Befehl mit Rueckmeldung ausfuehren
async function run(fn) {
  try {
    return await fn();
  } catch (e) {
    banner(e.message, 'error');
    return undefined;
  }
}

function bindActions() {
  $('connectBtn').addEventListener('click', () => (ui.lc ? disconnect() : chooseAndConnect()));
  $('setupBtn').addEventListener('click', () => run(async () => {
    ui.overlay = { kind: 'init', start: now() };
    try {
      await ui.lc.initialSetup();
    } finally {
      ui.overlay = null;
    }
  }));
  $('syncTime').addEventListener('click', () => run(() => ui.lc.setTime({ timechange: clockDst() })));
  $('setManual').addEventListener('click', () => run(async () => {
    const v = $('manualTime').value;
    const m = v.match(/^(\d{4})-(\d\d)-(\d\d)T(\d\d):(\d\d)(?::(\d\d))?/);
    if (!m) throw new Error('Bitte Datum und Uhrzeit eingeben.');
    const target = berlinToMs(...m.slice(1).map((x) => Number(x || 0)));
    await ui.lc.setTime({ offsetMs: target - Date.now(), timechange: clockDst() });
  }));
  $('dst').addEventListener('change', (e) => run(async () => {
    ui.draft.timechange = e.target.checked ? 1 : 0;
    if (usable()) {
      await ui.lc.setTimechange(e.target.checked);
      markSent(['timechange']);
    }
  }));
  $('applyColor').addEventListener('click', () => run(async () => {
    const d = ui.draft;
    if (d.mode === 0) await ui.lc.setColor(d.color);
    else await ui.lc.setMode(d.mode);
    markSent(['mode', 'color']);
  }));
  $('applyBright').addEventListener('click', () => run(async () => {
    await ui.lc.setBrightness(ui.draft);
    markSent(BRIGHT_KEYS);
  }));
  $('applyLdr').addEventListener('click', () => run(async () => {
    await ui.lc.setLdr(ui.draft.ldr);
    markSent(['ldr']);
  }));
  $('selftestBtn').addEventListener('click', () => run(async () => {
    const box = $('selftestResult');
    box.hidden = false;
    box.innerHTML = '<span class="tag">Läuft, etwa 30 Sekunden …</span>';
    const r = await ui.lc.selftest();
    const ldr = { LDROK: ['Lichtsensor OK', 'ok'], LDRTH: ['Lichtsensor: sehr hell / Kurzschluss?', 'bad'], LDRTL: ['Lichtsensor: sehr dunkel / nicht angeschlossen?', 'bad'], PROBLEM: ['Lichtsensor: Problem', 'bad'] }[r.ldr] ?? ['Lichtsensor: keine Antwort', 'bad'];
    const rtc = r.rtc === 'RTCOK' ? ['Echtzeituhr OK', 'ok'] : ['Echtzeituhr nicht gestellt', 'bad'];
    box.innerHTML = [ldr, rtc].map(([t, c]) => `<span class="tag ${c}">${t}</span>`).join('');
  }));
  $('factoryBtn').addEventListener('click', () => { $('confirmReset').hidden = false; });
  $('confirmNo').addEventListener('click', () => { $('confirmReset').hidden = true; });
  $('confirmYes').addEventListener('click', () => run(async () => {
    $('confirmReset').hidden = true;
    await ui.lc.factoryReset();
  }));
  $('rawForm').addEventListener('submit', (e) => {
    e.preventDefault();
    const v = $('raw').value.trim();
    if (!v) return;
    $('raw').value = '';
    run(() => ui.lc.command(v, { label: 'Befehl' }));
  });
  window.addEventListener('pagehide', () => { ui.lc?.close(); });
}

// ---------- Konsole ----------
// Zeilen gesammelt einfuegen: beim Start kommen viele auf einmal
const LOG_MAX = 400;
const logQueue = [];
function logLine({ text, dir }) {
  if (!logQueue.length) setTimeout(flushLog, 50);
  logQueue.push({ text, dir });
}

function flushLog() {
  const pre = $('log');
  const nearBottom = pre.scrollHeight - pre.scrollTop - pre.clientHeight < 30;
  const frag = document.createDocumentFragment();
  for (const { text, dir } of logQueue.splice(0).slice(-LOG_MAX)) {
    const span = document.createElement('span');
    span.className = dir === 'out' ? 'out' : dir === 'error' ? 'err' : '';
    span.textContent = `${dir === 'out' ? '→ ' : ''}${text}\n`;
    frag.appendChild(span);
  }
  pre.appendChild(frag);
  while (pre.childNodes.length > LOG_MAX) pre.firstChild.remove();
  if (nearBottom) pre.scrollTop = pre.scrollHeight;
}

// ---------- Start ----------
buildControls();
bindActions();
modelChanged();
setState('closed');
{
  const f = berlinFields(Date.now());
  $('manualTime').value = `${f.y}-${two(f.mo)}-${two(f.d)}T${hm(f.h, f.mi)}`;
}
if (!('serial' in navigator)) {
  $('connectBtn').disabled = true;
  banner(window.isSecureContext
    ? 'Dieser Browser kann nicht per USB mit der Uhr sprechen. Nimm Chrome, Edge oder Firefox (ab Version 151) am Computer. Die Vorschau funktioniert trotzdem.'
    : 'Web Serial braucht eine sichere Verbindung (https). Die Vorschau funktioniert trotzdem.');
}
render();
setInterval(render, 200);
