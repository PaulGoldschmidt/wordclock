// Die Frontplatte als SVG: echte Buchstabenumrisse aus der Druckvorlage, beleuchtet mit den LED-Farben.
import { PLATE } from './plate-data.js';
import { cellLed, CORNER_LEDS } from './wordclock.js';

const NS = 'http://www.w3.org/2000/svg';
const UNLIT = '#3a3435';
const CORNER_ORDER = ['tl', 'tr', 'br', 'bl']; // Reihenfolge der Punkte im PDF

function el(name, attrs = {}, parent) {
  const e = document.createElementNS(NS, name);
  for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
  parent?.appendChild(e);
  return e;
}

export const rgb = ([r, g, b]) => `rgb(${r},${g},${b})`;
// LED hinter Diffusor: Mitte heller als die Farbe
const core = ([r, g, b], k) => rgb([r, g, b].map((v) => Math.round(v + (255 - v) * k)));

export class Plate {
  constructor(svg) {
    const size = PLATE.size;
    svg.setAttribute('viewBox', `0 0 ${size} ${size}`);
    const defs = el('defs', {}, svg);
    const glow = el('filter', { id: 'p-glow', x: '-40%', y: '-40%', width: '180%', height: '180%' }, defs);
    el('feGaussianBlur', { stdDeviation: 7 }, glow);
    const halo = el('filter', { id: 'p-halo', x: '-60%', y: '-60%', width: '220%', height: '220%' }, defs);
    el('feGaussianBlur', { stdDeviation: 34 }, halo);
    const shade = el('radialGradient', { id: 'p-shade', cx: '50%', cy: '35%', r: '75%' }, defs);
    el('stop', { offset: '0%', 'stop-color': '#ffffff', 'stop-opacity': '0.045' }, shade);
    el('stop', { offset: '100%', 'stop-color': '#000000', 'stop-opacity': '0.18' }, shade);

    el('rect', { width: size, height: size, fill: PLATE.color }, svg);
    el('rect', { width: size, height: size, fill: 'url(#p-shade)' }, svg);
    const haloLayer = el('g', { filter: 'url(#p-halo)', class: 'p-halo' }, svg);
    const glowLayer = el('g', { filter: 'url(#p-glow)', class: 'p-glowl' }, svg);
    const letterLayer = el('g', {}, svg);

    this.cells = new Map(); // LED-Nummer -> { path, glow, halo }
    const half = PLATE.pitch / 2;
    PLATE.letters.forEach((row, r) => row.forEach((d, c) => {
      const cx = PLATE.origin + c * PLATE.pitch;
      const cy = PLATE.origin + r * PLATE.pitch;
      this.cells.set(cellLed(r, c), {
        halo: el('circle', { cx, cy, r: half * 0.9, fill: 'transparent' }, haloLayer),
        glow: el('path', { d, fill: 'transparent' }, glowLayer),
        path: el('path', { d, fill: UNLIT, class: 'p-letter' }, letterLayer),
      });
    }));
    PLATE.dots.forEach(([cx, cy, rad], i) => {
      this.cells.set(CORNER_LEDS[CORNER_ORDER[i]], {
        halo: el('circle', { cx, cy, r: rad * 5, fill: 'transparent' }, haloLayer),
        glow: el('circle', { cx, cy, r: rad * 1.6, fill: 'transparent' }, glowLayer),
        path: el('circle', { cx, cy, r: rad, fill: UNLIT, class: 'p-letter' }, letterLayer),
      });
    });
  }

  // leds: 125 x ([r,g,b] | null); level: 0..1 sichtbare Helligkeit
  set(leds, level = 1) {
    const lit = [];
    for (const [k, c] of this.cells) {
      const v = leds[k];
      if (v && (v[0] | v[1] | v[2])) {
        const scaled = v.map((x) => Math.round(x * (0.35 + 0.65 * level)));
        c.path.style.fill = core(scaled, 0.3 * level);
        c.glow.style.fill = rgb(scaled);
        c.glow.style.opacity = String(0.55 + 0.45 * level);
        c.halo.style.fill = rgb(v);
        c.halo.style.opacity = String(0.12 + 0.38 * level);
        lit.push(v);
      } else {
        c.path.style.fill = '';
        c.glow.style.fill = 'transparent';
        c.halo.style.fill = 'transparent';
      }
    }
    // Mittlere Farbe fuer den Lichtschein an der "Wand" hinter der Platte
    if (!lit.length) return null;
    const avg = [0, 1, 2].map((i) => Math.round(lit.reduce((s, v) => s + v[i], 0) / lit.length));
    return { color: avg, amount: Math.min(1, lit.length / 22) * level };
  }

  // Buchstaben hervorheben (Hinweis beim Ueberfahren der Abschnitte), cells = [[row, col], ...]
  hint(cells) {
    const want = new Set(cells.map(([r, c]) => cellLed(r, c)));
    for (const [k, c] of this.cells) c.path.classList.toggle('p-hint', want.has(k));
  }
}

// Kleine Punkt-Ansicht fuer die Farbmodus-Kacheln
export function miniPlate(svg, leds) {
  svg.setAttribute('viewBox', '0 0 130 130');
  svg.replaceChildren();
  el('rect', { width: 130, height: 130, rx: 4, fill: PLATE.color }, svg);
  for (let r = 0; r < 11; r++) {
    for (let c = 0; c < 11; c++) {
      const v = leds[cellLed(r, c)];
      el('circle', { cx: 20 + c * 9, cy: 20 + r * 9, r: v ? 3.2 : 2, fill: v ? rgb(v) : UNLIT }, svg);
    }
  }
  const pos = { tl: [8, 8], tr: [122, 8], br: [122, 122], bl: [8, 122] };
  for (const [name, [x, y]] of Object.entries(pos)) {
    const v = leds[CORNER_LEDS[name]];
    el('circle', { cx: x, cy: y, r: v ? 2.6 : 1.6, fill: v ? rgb(v) : UNLIT }, svg);
  }
}
