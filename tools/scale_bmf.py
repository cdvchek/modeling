# Makes a smaller .bmf from a larger one by trimming each cell and area-averaging it down.
# Usage: python tools/scale_bmf.py <input.bmf> <output.bmf> <width> <height> [left top right bottom]
# The optional crop box (in input pixels, right/bottom exclusive) trims empty cell margins first.
# The UI font is made with:
#   python tools/scale_bmf.py assets/fonts/console.bmf assets/fonts/ui.bmf 10 16 1 1 15 23

import sys
from PIL import Image

HEX = "0123456789ABCDEF"


def load(path):
    lines = open(path).read().split("\n")
    width = int(lines[1].split()[1])
    height = int(lines[2].split()[1])

    glyphs = {}
    i = 3
    while i < len(lines):
        if lines[i].strip().isdigit():
            code = int(lines[i])
            image = Image.new("L", (width, height))
            for y, row in enumerate(lines[i + 1:i + 1 + height]):
                for x, digit in enumerate(row):
                    image.putpixel((x, y), int(digit, 16) * 17)
            glyphs[code] = image
            i += 1 + height
        else:
            i += 1

    return width, height, glyphs


def main():
    source, output = sys.argv[1], sys.argv[2]
    width, height = int(sys.argv[3]), int(sys.argv[4])

    source_width, source_height, glyphs = load(source)
    crop = tuple(int(v) for v in sys.argv[5:9]) if len(sys.argv) >= 9 else (0, 0, source_width, source_height)

    lines = ["BMF1", f"width {width}", f"height {height}", ""]

    for code in sorted(glyphs):
        small = glyphs[code].crop(crop).resize((width, height), Image.BOX)

        lines.append(str(code))
        for y in range(height):
            lines.append("".join(HEX[round(small.getpixel((x, y)) / 17)] for x in range(width)))
        lines.append("")

    with open(output, "w", newline="\n") as file:
        file.write("\n".join(lines))


if __name__ == "__main__":
    main()
