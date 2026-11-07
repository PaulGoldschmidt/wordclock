#!/usr/bin/env python3
"""Erzeugt web/js/plate-data.js aus der Frontplatte (PDF aus Illustrator).

Die Buchstaben werden als echte Umrisse uebernommen, damit die Vorschau auf der Webseite
genau wie die Platte aussieht. Benoetigt PyMuPDF:  pip install pymupdf

    python3 tools/extract_frontplate.py <frontplatte>.pdf web/js/plate-data.js
"""
import json
import sys

import fitz  # PyMuPDF

def num(v):
    return f"{v:.1f}".rstrip("0").rstrip(".")


def svg_path(drawing):
    out, cur = [], None
    for item in drawing["items"]:
        kind = item[0]
        if kind == "re":  # das "I" ist ein reines Rechteck
            r = item[1]
            out.append(f"M{num(r.x0)} {num(r.y0)}H{num(r.x1)}V{num(r.y1)}H{num(r.x0)}Z")
            cur = None
            continue
        start = item[1]
        if cur is None or abs(cur.x - start.x) > 0.01 or abs(cur.y - start.y) > 0.01:
            if out and not out[-1].endswith("Z"):
                out.append("Z")
            out.append(f"M{num(start.x)} {num(start.y)}")
        if kind == "l":
            end = item[2]
            out.append(f"L{num(end.x)} {num(end.y)}")
        elif kind == "c":
            c1, c2, end = item[2], item[3], item[4]
            out.append(f"C{num(c1.x)} {num(c1.y)} {num(c2.x)} {num(c2.y)} {num(end.x)} {num(end.y)}")
        else:
            raise SystemExit(f"unerwartetes Pfadelement {kind!r}")
        cur = end
    if out and not out[-1].endswith("Z"):
        out.append("Z")
    return "".join(out)


def main(pdf, target):
    page = fitz.open(pdf)[0]
    drawings = page.get_drawings()
    white = (1.0, 1.0, 1.0)
    letters = [d for d in drawings if d["type"] == "f" and d["fill"] == white and d["rect"].width < 200]
    letters = letters[: len(letters) // 2]  # die Buchstaben liegen doppelt im PDF (zwei Druckebenen)
    # Rasterabstand aus den Buchstabenmitten ableiten (11 x 11 Zeichen)
    centers_x = sorted((d["rect"].x0 + d["rect"].x1) / 2 for d in letters)
    first, last = centers_x[0], centers_x[-1]
    pitch = (last - first) / 10
    cells = {}
    for d in letters:
        cx = (d["rect"].x0 + d["rect"].x1) / 2
        cy = (d["rect"].y0 + d["rect"].y1) / 2
        col, row = round((cx - first) / pitch), round((cy - first) / pitch)
        if (row, col) in cells or not (0 <= row < 11 and 0 <= col < 11):
            raise SystemExit(f"Buchstabe bei ({cx:.0f}, {cy:.0f}) passt nicht ins Raster")
        cells[(row, col)] = svg_path(d)
    if len(cells) != 121:
        raise SystemExit(f"{len(cells)} statt 121 Buchstaben gefunden")
    dots = [d for d in drawings if d["type"] == "fs" and d["rect"].width < 50][:4]
    plate = next(d for d in drawings if d["type"] == "f" and d["rect"].width > 200)
    data = {
        "size": round(page.rect.width, 1),
        "color": "#%02x%02x%02x" % tuple(round(c * 255) for c in plate["fill"]),
        "pitch": round(pitch, 2),
        "origin": round(first, 1),
        "letters": [[cells[(r, c)] for c in range(11)] for r in range(11)],
        "dots": [[round((d["rect"].x0 + d["rect"].x1) / 2, 1), round((d["rect"].y0 + d["rect"].y1) / 2, 1),
                  round(d["rect"].width / 2, 1)] for d in dots],
    }
    with open(target, "w", encoding="utf-8") as f:
        f.write("// Automatisch erzeugt von tools/extract_frontplate.py aus der Druckvorlage der Frontplatte (PDF).\n")
        f.write("// Bitte nicht von Hand bearbeiten.\n")
        f.write("export const PLATE = ")
        json.dump(data, f, ensure_ascii=False, separators=(",", ":"))
        f.write(";\n")
    print(f"{target}: 121 Buchstaben, {len(dots)} Eckpunkte, Raster {pitch:.2f} pt")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    main(sys.argv[1], sys.argv[2])
