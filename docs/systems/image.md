# Image files

Reading and writing pictures for the suite: Valuma's reference images and textures, and textures in the Aevora engine (exported `.vlmobj` files carry PNGs). PNG only for now.

Files: [shared/image/](../../shared/image/), built as the `image` static library. Like `shared/vlmobj/`, it uses only the C++ standard library, so the engine can link it too. `modeling_core` links it.

## API

[image.hpp](../../shared/image/image.hpp), namespace `image`

| Function / type | Description |
|---|---|
| `Image` | `width`, `height`, and `pixels`: 8-bit RGBA, rows from the top, no padding (`width × height × 4` bytes). |
| `decodePng(data, size, out, error)` | Decodes any PNG that isn't interlaced into `out`. On failure returns false with `error` saying why, in words a user can read ("the file is damaged (a chunk's checksum is wrong)"), and leaves `out` empty. |
| `encodePng(image)` | An RGBA8 PNG of the image ([png_write.cpp](../../shared/image/png_write.cpp)): `IHDR`, one `IDAT`, `IEND`, rows unfiltered, in stored (uncompressed) DEFLATE blocks of up to 65,535 bytes with the Adler-32 and every chunk's CRC-32. Big but simple; a compressor comes with paintable layers. |
| `isPng(data, size)` | True when the bytes start with the PNG signature. |
| `inflateZlib(data, size, out, error, maxSize)` | zlib-wrapped DEFLATE, as PNG stores its pixels. Fails rather than produce more than `maxSize` bytes. |
| `MAX_DIMENSION` | 16384: wider or taller images are refused (no GPU holds a bigger texture anyway). |

## PNG ([png.cpp](../../shared/image/png.cpp))

- **Chunks:** every chunk's CRC-32 is checked. `IHDR` must come first; `PLTE`, `tRNS`, the `IDAT` run (joined before decompressing), and `IEND` are read. Other chunks with a lowercase first letter (text, gamma, color profiles, …) are skipped; an unknown one with an uppercase first letter means the image can't be shown without it, so the file is refused.
- **Formats:** every color type at every bit depth PNG allows: grey (1, 2, 4, 8, 16 bits), RGB (8, 16), palette (1, 2, 4, 8), grey + alpha (8, 16), RGBA (8, 16). All become 8-bit RGBA: low depths stretch to the full range (a 2-bit 3 becomes 255), 16-bit samples keep their high byte.
- **Transparency:** `tRNS` gives palette entries their alpha, or names the one grey value or RGB color that's fully clear (compared at the file's own depth).
- **Rows:** each row's filter (none, sub, up, average, Paeth) is undone in place, using whole pixels (or one byte, below 8 bits per pixel) as the step back.
- **Checks:** sizes and depths against the spec, data that decompresses to exactly `(stride + 1) × height` bytes, palette indices inside the palette. Interlaced images are refused with a message saying so.

## DEFLATE ([inflate.cpp](../../shared/image/inflate.cpp))

Stored, fixed-Huffman, and dynamic-Huffman blocks, as RFC 1951 describes, inside the zlib wrapper (RFC 1950: header check, no preset dictionary, Adler-32 of the output at the end). Huffman codes are decoded canonically a bit at a time against the count of codes of each length, as zlib's `puff` does: short and easy to check, and fast enough for reference images. Over-subscribed code lengths, back-references before the start, and output past the expected size all fail cleanly. The fixed-code tables are built once, safely across threads.

## Later

- Interlaced (Adam7) PNGs.
- JPEG.
- A faster table-driven Huffman decoder, if big textures make loading slow.
- Compression in `encodePng` (Huffman and back-references, with row filters chosen per row).

## Tests

[shared/image/tests/](../../shared/image/tests/): `image_tests.cpp` checks every fixture against the pixels it must decode to, plus damaged files. The fixtures in `fixtures/` are written by `make_fixtures.py` (standard library only, run from anywhere), which encodes them by hand so every color type, depth, and row filter appears, and saves each one's expected RGBA as `<name>.rgba` (width and height, then the pixels). See [testing.md](../testing.md).
