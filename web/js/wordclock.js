// Re-implements the firmware's display (WordCore/LED_Routine.ino + LED_Layout.ino) LED by LED.
// Colour modes 1-3 depend on the order in which the LEDs are set, so this is deliberately a literal
// translation, not a cleaner rewrite.

export const NUM_LEDS = 125;

// The rows are wired as a serpentine: even rows run left to right, odd rows right to left.
export function cellLed(row, col) {
  return row * 11 + (row % 2 === 0 ? col : 10 - col);
}

// Minute dots in the corners. The firmware lights them in the order 123, 122, 124, 121.
// Their positions are assumed (the chain continues from LED 120, bottom right, anticlockwise round the plate):
export const CORNER_LEDS = { tl: 123, tr: 122, bl: 124, br: 121 };

// "WORDCLOCK" as a diagonal: the logo the clock shows at startup and as a status display (effects.ino)
export const LOGO_LEDS = [20, 24, 40, 48, 60, 72, 80, 96, 100];
const LOGO_RAINBOW = [
  [0, 255, 0], [0, 168, 87], [0, 81, 174], [0, 0, 255], [87, 0, 168],
  [174, 0, 81], [255, 0, 0], [174, 87, 0], [81, 174, 0],
];

const ROW_COLORS = [
  [255, 0, 0], [186, 69, 0], [117, 138, 0], [48, 207, 0], [0, 234, 21], [0, 165, 90],
  [0, 96, 159], [0, 27, 228], [42, 0, 213], [111, 0, 144], [180, 0, 75],
];

const HOUR_WORDS = [
  [66, 70], [86, 83], [109, 106], [64, 61], [73, 76], [51, 54],
  [94, 98], [111, 116], [58, 55], [88, 91], [104, 101], [79, 77],
];

export const FACTORY = {
  color: [0, 204, 255], mode: 0, dayMax: 255, dayMin: 35, nightMax: 60, nightMin: 7,
  nightBegin: 23, nightEnd: 6, timechange: 1, ldr: 3,
};

// Night window (LDR_EEPROM.ino). The original firmware 6.1 (legacy) only handles a window across
// midnight correctly; 6.2 also handles one within a single day.
export function isNight(h, begin, end, legacy = false) {
  if (legacy) return begin <= h || end > h;
  return begin > end ? h >= begin || h < end : h >= begin && h < end;
}

export class WordClockModel {
  // State right after the matching command: '#/' sets the colour, '#%', '#=' and '#!' start at red.
  constructor(color = FACTORY.color, mode = 0) {
    this.mode = mode;
    [this.R, this.G, this.B] = mode === 0 ? color : [255, 0, 0];
  }

  // LED_Routine(): returns 125 entries, each [r, g, b] or null (off).
  render(stunde, minute) {
    const leds = new Array(NUM_LEDS).fill(null);
    this._leds = leds;
    const modMinute = minute % 5;
    const cMinute = minute - modMinute;
    let cStunde = stunde;
    if (minute >= 25) {
      cStunde = (stunde === 0 || stunde === 12 || stunde === 24) ? 1 : stunde + 1;
    }
    if (this.mode === 1 && minute !== this._lastMinute) this._colorCycle();
    this._lastMinute = minute;
    if (this.mode === 2) { this.R = 255; this.G = 0; this.B = 0; }

    this._esist();
    if ([5, 25, 35, 55].includes(cMinute)) this._word(7, 10); // FUENF
    if ([20, 40].includes(cMinute)) this._word(21, 15); // ZWANZIG
    if ([10, 50].includes(cMinute)) this._word(14, 11); // ZEHN
    if ([15, 45].includes(cMinute)) this._word(25, 31); // VIERTEL
    if ([25, 40, 45, 50, 55].includes(cMinute)) this._word(39, 37); // VOR
    if ([5, 10, 15, 20, 35].includes(cMinute)) this._word(36, 33); // NACH
    if ([25, 30, 35].includes(cMinute)) this._word(44, 47); // HALB
    if (cStunde % 12 === 1 && cMinute === 0) {
      this._word(86, 84); // EIN without the S: "ES IST EIN UHR"
    } else {
      const [a, b] = HOUR_WORDS[cStunde % 12];
      this._word(a, b);
    }
    if (cMinute === 0) this._word(118, 120); // UHR
    if (modMinute >= 1) this._rand(modMinute);
    return leds;
  }

  _set(k) {
    this._leds[k] = [this.R & 0xff, this.G & 0xff, this.B & 0xff]; // setRGB() takes uint8_t
  }

  _word(from, to) {
    let k = from;
    for (;;) {
      this._colorLED(k);
      this._set(k);
      if (k === to) break;
      k += from < to ? 1 : -1;
    }
  }

  _esist() {
    const letters = [0, 1, 3, 4, 5];
    letters.forEach((led, k) => {
      if (k > 0 || this.mode === 3) this._colorLED(led);
      this._set(led);
    });
  }

  _colorCycle() { // ColorCycle(): mode 1, one step per minute
    if (this.R > 0 && this.B === 0) { this.R -= 1; this.G += 1; }
    if (this.G > 0 && this.R === 0) { this.G -= 1; this.B += 1; }
    if (this.B > 0 && this.G === 0) { this.R += 1; this.B -= 1; }
  }

  _colorLED(pos) { // ColorLEDCycle()
    if (this.mode === 2) {
      if (this.R > 0 && this.B === 0) { this.R -= 64; this.G += 64; }
      if (this.G > 0 && this.R === 0) { this.G -= 64; this.B += 64; }
      if (this.B > 0 && this.G === 0) { this.R += 64; this.B -= 64; }
      if (this.R > 255) { this.G = this.R - 255; this.R = 255; }
      if (this.G > 255) { this.B = this.G - 255; this.G = 255; }
      if (this.B > 255) { this.R = this.B - 255; this.B = 255; }
      if (this.R < 0) { this.G = 255 + this.R; this.R = 0; }
      if (this.G < 0) { this.B = 255 + this.G; this.G = 0; }
      if (this.B < 0) { this.R = 255 + this.B; this.B = 0; }
    } else if (this.mode === 3 && pos <= 120) {
      [this.R, this.G, this.B] = ROW_COLORS[Math.floor(pos / 11)];
    }
  }

  _rand(n) { // RAND1()..RAND4(): each level calls the previous one first
    const m = this.mode;
    if (m === 2 || m === 3) { this.R = 255; this.G = 0; this.B = 0; }
    this._set(123);
    if (n === 1) return;
    if (m === 2) { this.R = 64; this.G = 191; this.B = 0; }
    this._set(122);
    if (n === 2) return;
    if (m === 2) { this.R = 0; this.G = 147; this.B = 127; } else if (m === 3) { this.R = 180; this.G = 0; this.B = 75; }
    this._set(124);
    if (n === 3) return;
    if (m === 2) { this.R = 63; this.G = 0; this.B = 192; } else if (m === 3) { this.R = 180; this.G = 0; this.B = 75; }
    this._set(121);
    if (m === 3) { this.R = 255; this.G = 0; this.B = 0; }
  }
}

// Status displays of the clock (effects.ino)
export function logoFrame(color) { // no colour: WordClock() rainbow logo, otherwise WordClock_color()
  const leds = new Array(NUM_LEDS).fill(null);
  LOGO_LEDS.forEach((k, i) => { leds[k] = color ? [...color] : [...LOGO_RAINBOW[i]]; });
  return leds;
}

export function fillFrame(color, count = NUM_LEDS) {
  const leds = new Array(NUM_LEDS).fill(null);
  for (let k = 0; k < count; k++) leds[k] = [...color];
  return leds;
}

// The time in words as the plate shows it, for screen readers and the caption
export function phrase(stunde, minute) {
  const names = ['zwölf', 'eins', 'zwei', 'drei', 'vier', 'fünf', 'sechs', 'sieben', 'acht', 'neun', 'zehn', 'elf'];
  const c = minute - (minute % 5);
  const h = minute >= 25 ? (stunde + 1) % 12 : stunde % 12;
  const pre = {
    0: '', 5: 'fünf nach ', 10: 'zehn nach ', 15: 'viertel nach ', 20: 'zwanzig nach ', 25: 'fünf vor halb ',
    30: 'halb ', 35: 'fünf nach halb ', 40: 'zwanzig vor ', 45: 'viertel vor ', 50: 'zehn vor ', 55: 'fünf vor ',
  }[c];
  const hour = c === 0 ? (h === 1 ? 'ein' : names[h]) + ' Uhr' : names[h];
  const dots = minute % 5 ? ` (+${minute % 5})` : '';
  return `Es ist ${pre}${hour}${dots}`;
}
