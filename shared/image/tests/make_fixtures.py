# Writes the PNG fixtures the image tests read, each with the RGBA it must decode to (<name>.rgba).
# Standard library only: the PNGs are encoded here by hand so every color type, depth, and filter is covered.
# Run from anywhere: python shared/image/tests/make_fixtures.py
import os
import random
import struct
import zlib

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fixtures')

def chunk(kind, body):
    return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body) & 0xFFFFFFFF)

def paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c

def filter_rows(rows, bpp):
    # Row y uses filter y % 5 so all five appear in every image taller than 4 rows
    out = bytearray()
    prev = bytes(len(rows[0]))
    for y, row in enumerate(rows):
        f = y % 5
        out.append(f)
        for i, x in enumerate(row):
            a = row[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            pred = [0, a, b, (a + b) >> 1, paeth(a, b, c)][f]
            out.append((x - pred) & 0xFF)
        prev = row
    return bytes(out)

def pack_samples(samples, depth):
    if depth == 8:
        return bytes(samples)
    if depth == 16:
        return b''.join(struct.pack('>H', s) for s in samples)
    out = bytearray()
    per = 8 // depth
    for i in range(0, len(samples), per):
        byte = 0
        for k, s in enumerate(samples[i:i + per]):
            byte |= s << (8 - depth * (k + 1))
        out.append(byte)
    return bytes(out)

def to8(v, depth):
    return {1: v * 255, 2: v * 85, 4: v * 17, 8: v, 16: v >> 8}[depth]

def write_png(name, width, height, color, depth, samples, palette=None, trns=None, level=9, interlace=0, expected=True):
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[color]
    bpp = max(1, channels * depth // 8)
    rows = [pack_samples(samples[y * width * channels:(y + 1) * width * channels], depth) for y in range(height)]
    raw = filter_rows(rows, bpp)

    png = b'\x89PNG\r\n\x1a\n'
    png += chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, depth, color, 0, 0, interlace))
    png += chunk(b'tEXt', b'Comment\x00skipped by the reader')
    if palette is not None:
        png += chunk(b'PLTE', bytes(c for rgb in palette for c in rgb))
    if trns is not None:
        png += chunk(b'tRNS', trns)
    data = zlib.compress(raw, level)
    # Split the data over several IDAT chunks, as encoders do
    for i in range(0, len(data), 97):
        png += chunk(b'IDAT', data[i:i + 97])
    png += chunk(b'IEND', b'')

    rgba = bytearray()
    for p in range(width * height):
        s = samples[p * channels:(p + 1) * channels]
        if color == 0:
            g = to8(s[0], depth)
            alpha = 0 if (trns is not None and s[0] == struct.unpack('>H', trns)[0]) else 255
            rgba += bytes([g, g, g, alpha])
        elif color == 2:
            alpha = 0 if (trns is not None and tuple(s) == struct.unpack('>HHH', trns)) else 255
            rgba += bytes([to8(s[0], depth), to8(s[1], depth), to8(s[2], depth), alpha])
        elif color == 3:
            alpha = trns[s[0]] if trns is not None and s[0] < len(trns) else 255
            rgba += bytes(palette[s[0]]) + bytes([alpha])
        elif color == 4:
            g = to8(s[0], depth)
            rgba += bytes([g, g, g, to8(s[1], depth)])
        else:
            rgba += bytes(to8(v, depth) for v in s)

    with open(os.path.join(OUT, name + '.png'), 'wb') as f:
        f.write(png)
    if expected:
        with open(os.path.join(OUT, name + '.rgba'), 'wb') as f:
            f.write(struct.pack('<II', width, height) + bytes(rgba))

def main():
    os.makedirs(OUT, exist_ok=True)
    rng = random.Random(7)
    w, h = 13, 7   # odd width so packed rows end mid-byte

    def rand(count, depth):
        return [rng.randrange(1 << depth) for _ in range(count)]

    for depth in (1, 2, 4, 8, 16):
        write_png(f'grey{depth}', w, h, 0, depth, rand(w * h, depth))
    for depth in (8, 16):
        write_png(f'rgb{depth}', w, h, 2, depth, rand(w * h * 3, depth))
        write_png(f'greyalpha{depth}', w, h, 4, depth, rand(w * h * 2, depth))
        write_png(f'rgba{depth}', w, h, 6, depth, rand(w * h * 4, depth))
    for depth in (1, 2, 4, 8):
        colors = min(1 << depth, 200)
        palette = [(rng.randrange(256), rng.randrange(256), rng.randrange(256)) for _ in range(colors)]
        write_png(f'palette{depth}', w, h, 3, depth, [rng.randrange(colors) for _ in range(w * h)], palette)

    # Transparency: a grey value, an RGB color, and alpha for the first palette entries
    grey = rand(w * h, 4)
    write_png('grey4_trns', w, h, 0, 4, grey, trns=struct.pack('>H', grey[0]))
    rgb = rand(w * h * 3, 8)
    rgb[3:6] = rgb[0:3]
    write_png('rgb8_trns', w, h, 2, 8, rgb, trns=struct.pack('>HHH', *rgb[0:3]))
    palette = [(i * 16, 255 - i * 16, i * 7) for i in range(16)]
    write_png('palette4_trns', w, h, 3, 4, [rng.randrange(16) for _ in range(w * h)], palette, trns=bytes([0, 64, 128]))

    # Compression: stored blocks (level 0), and a larger image that needs dynamic blocks and long back-references
    write_png('stored', w, h, 6, 8, rand(w * h * 4, 8), level=0)
    big_w, big_h = 300, 200
    gradient = [((x * 255) // big_w, (y * 255) // big_h, ((x ^ y) & 0xFF), 255) for y in range(big_h) for x in range(big_w)]
    # The test computes its pixels, so there's no 240 KB .rgba beside it
    write_png('gradient', big_w, big_h, 6, 8, [c for p in gradient for c in p], expected=False)

    # Interlaced: refused with a clear message (no .rgba)
    write_png('interlaced', 4, 4, 6, 8, rand(64, 8), interlace=1, expected=False)
    print('fixtures written to', OUT)

if __name__ == '__main__':
    main()
