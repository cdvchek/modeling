"""Shift every glyph in a .bmf font onto a shared baseline.

Glyph shapes are left untouched; each glyph only moves vertically. Running it
on an already-aligned font changes nothing, so it's safe to rerun after
editing or adding glyphs.

Usage: python tools/fix_bmf_baseline.py [path]   (default: assets/fonts/console.bmf)
"""

import sys

WIDTH, HEIGHT = 16, 24

# Rows in the 24-row cell, measured from the console font:
# capitals are 14 rows, lowercase x-height is 10, descenders are 4.
CAP_TOP = 3
X_TOP = 7
BASELINE = 16          # last ink row of anything sitting on the baseline
DESC_BOTTOM = 20
X_MIDDLE = (X_TOP + BASELINE) / 2
TALL_MIDDLE = (CAP_TOP + DESC_BOTTOM) / 2


def target_top(char, height):
    """Row the glyph's top ink row should move to."""
    if char in "gpqyj":
        return DESC_BOTTOM - height + 1
    if char == "Q":
        return CAP_TOP
    if char == ",":
        return BASELINE - 2            # head level with '.', tail 2 rows below
    if char == ";":
        return X_TOP                   # dot at x-height, tail 2 rows below
    if char in "'\"^`*":
        return CAP_TOP
    if char in "-+=<>~":
        return round(X_MIDDLE - (height - 1) / 2)
    if char == "_":
        return DESC_BOTTOM - height
    if char in "()[]{}|/\\$@":
        return round(TALL_MIDDLE - (height - 1) / 2)
    return BASELINE - height + 1


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "assets/fonts/console.bmf"
    tokens = open(path).read().split()

    if tokens[:5] != ["BMF1", "width", str(WIDTH), "height", str(HEIGHT)]:
        sys.exit(f"{path}: not a {WIDTH}x{HEIGHT} BMF1 font")

    out = ["BMF1", f"width {WIDTH}", f"height {HEIGHT}", ""]
    blank = "0" * WIDTH

    i = 5
    while i < len(tokens):
        code = int(tokens[i])
        rows = tokens[i + 1:i + 1 + HEIGHT]
        i += 1 + HEIGHT

        ink = [y for y in range(HEIGHT) if rows[y] != blank]
        if ink:
            shift = target_top(chr(code), ink[-1] - ink[0] + 1) - ink[0]
            if ink[0] + shift < 0 or ink[-1] + shift >= HEIGHT:
                sys.exit(f"glyph {chr(code)!r} would not fit in the cell")
            rows = [rows[y - shift] if 0 <= y - shift < HEIGHT else blank for y in range(HEIGHT)]

        out.append(str(code))
        out.extend(rows)
        out.append("")

    with open(path, "w", newline="\n") as f:
        f.write("\n".join(out))


if __name__ == "__main__":
    main()
