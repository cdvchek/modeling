#include "image/image.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>

// PNG as the W3C spec describes it: chunks, each with a CRC; IHDR first, then the palette and transparency,
// the image data split over IDAT chunks, and IEND. Chunks only viewers care about (gamma, text, ...) are skipped.

namespace image {
    namespace {
        constexpr u8 SIGNATURE[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };

        enum ColorType : u8 { Grey = 0, Rgb = 2, Palette = 3, GreyAlpha = 4, Rgba = 6 };

        u32 readU32(const u8* p) {
            return (u32(p[0]) << 24) | (u32(p[1]) << 16) | (u32(p[2]) << 8) | u32(p[3]);
        }

        u32 readU16(const u8* p) {
            return (u32(p[0]) << 8) | u32(p[1]);
        }

        // The same CRC-32 as zip and the project files; kept here so this library depends on nothing
        u32 crc32(const u8* data, std::size_t size) {
            static const auto table = [] {
                std::array<u32, 256> t {};
                for (u32 i = 0; i < 256; ++i) {
                    u32 c = i;
                    for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
                    t[i] = c;
                }
                return t;
            }();
            u32 crc = 0xFFFFFFFFu;
            for (std::size_t i = 0; i < size; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
            return crc ^ 0xFFFFFFFFu;
        }

        bool fail(std::string& error, const char* why) {
            error = why;
            return false;
        }

        int channelsOf(u8 colorType) {
            switch (colorType) {
                case Grey: return 1;
                case Rgb: return 3;
                case Palette: return 1;
                case GreyAlpha: return 2;
                case Rgba: return 4;
                default: return 0;
            }
        }

        bool validDepth(u8 colorType, u8 depth) {
            switch (colorType) {
                case Grey: return depth == 1 || depth == 2 || depth == 4 || depth == 8 || depth == 16;
                case Palette: return depth == 1 || depth == 2 || depth == 4 || depth == 8;
                case Rgb:
                case GreyAlpha:
                case Rgba: return depth == 8 || depth == 16;
                default: return false;
            }
        }

        u8 paeth(int a, int b, int c) {
            const int p = a + b - c;
            const int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
            if (pa <= pb && pa <= pc) return static_cast<u8>(a);
            if (pb <= pc) return static_cast<u8>(b);
            return static_cast<u8>(c);
        }

        // Undoes each row's filter in place; rows are [filter byte][stride bytes]
        bool unfilter(std::vector<u8>& data, u32 height, std::size_t stride, std::size_t bpp, std::string& error) {
            const u8* previous = nullptr;
            for (u32 y = 0; y < height; ++y) {
                u8* row = data.data() + y * (stride + 1);
                const u8 filter = row[0];
                u8* line = row + 1;

                switch (filter) {
                    case 0:
                        break;
                    case 1:
                        for (std::size_t i = bpp; i < stride; ++i) line[i] = static_cast<u8>(line[i] + line[i - bpp]);
                        break;
                    case 2:
                        if (previous) for (std::size_t i = 0; i < stride; ++i) line[i] = static_cast<u8>(line[i] + previous[i]);
                        break;
                    case 3:
                        for (std::size_t i = 0; i < stride; ++i) {
                            const int left = i >= bpp ? line[i - bpp] : 0;
                            const int up = previous ? previous[i] : 0;
                            line[i] = static_cast<u8>(line[i] + ((left + up) >> 1));
                        }
                        break;
                    case 4:
                        for (std::size_t i = 0; i < stride; ++i) {
                            const int left = i >= bpp ? line[i - bpp] : 0;
                            const int up = previous ? previous[i] : 0;
                            const int upLeft = (previous && i >= bpp) ? previous[i - bpp] : 0;
                            line[i] = static_cast<u8>(line[i] + paeth(left, up, upLeft));
                        }
                        break;
                    default:
                        return fail(error, "the image data is damaged (bad row filter)");
                }
                previous = line;
            }
            return true;
        }

        struct Header {
            u32 width = 0;
            u32 height = 0;
            u8 depth = 0;
            u8 colorType = 0;
        };

        struct Transparency {
            bool present = false;
            u32 grey = 0;           // Grey: the sample value that is transparent
            u32 rgb[3] = {};        // Rgb: the color that is transparent
        };

        // The sample at index i of a row with samples of depth bits, as read (not scaled)
        u32 sampleAt(const u8* line, std::size_t i, u8 depth) {
            switch (depth) {
                case 8: return line[i];
                case 16: return readU16(line + i * 2);
                default: {
                    const std::size_t bit = i * depth;
                    const u32 shift = 8 - depth - static_cast<u32>(bit & 7);
                    return (line[bit >> 3] >> shift) & ((1u << depth) - 1u);
                }
            }
        }

        // A sample scaled to 0-255: low depths stretch to the full range, 16 bits keep the high byte
        u8 to8(u32 value, u8 depth) {
            switch (depth) {
                case 1: return value ? 255 : 0;
                case 2: return static_cast<u8>(value * 85);
                case 4: return static_cast<u8>(value * 17);
                case 16: return static_cast<u8>(value >> 8);
                default: return static_cast<u8>(value);
            }
        }

        void toRgba(const std::vector<u8>& data, const Header& h, std::size_t stride, const std::vector<u8>& palette,
                    const std::vector<u8>& paletteAlpha, const Transparency& trns, std::vector<u8>& out) {
            const int channels = channelsOf(h.colorType);
            out.resize(static_cast<std::size_t>(h.width) * h.height * 4);
            u8* dst = out.data();

            for (u32 y = 0; y < h.height; ++y) {
                const u8* line = data.data() + y * (stride + 1) + 1;
                for (u32 x = 0; x < h.width; ++x, dst += 4) {
                    const std::size_t s = static_cast<std::size_t>(x) * channels;
                    switch (h.colorType) {
                        case Grey: {
                            const u32 v = sampleAt(line, s, h.depth);
                            dst[0] = dst[1] = dst[2] = to8(v, h.depth);
                            dst[3] = (trns.present && v == trns.grey) ? 0 : 255;
                            break;
                        }
                        case Rgb: {
                            const u32 r = sampleAt(line, s, h.depth), g = sampleAt(line, s + 1, h.depth), b = sampleAt(line, s + 2, h.depth);
                            dst[0] = to8(r, h.depth);
                            dst[1] = to8(g, h.depth);
                            dst[2] = to8(b, h.depth);
                            dst[3] = (trns.present && r == trns.rgb[0] && g == trns.rgb[1] && b == trns.rgb[2]) ? 0 : 255;
                            break;
                        }
                        case Palette: {
                            // Indices past the palette were rejected before conversion
                            const u32 index = sampleAt(line, s, h.depth);
                            std::memcpy(dst, palette.data() + index * 3, 3);
                            dst[3] = index < paletteAlpha.size() ? paletteAlpha[index] : 255;
                            break;
                        }
                        case GreyAlpha:
                            dst[0] = dst[1] = dst[2] = to8(sampleAt(line, s, h.depth), h.depth);
                            dst[3] = to8(sampleAt(line, s + 1, h.depth), h.depth);
                            break;
                        default:
                            for (int c = 0; c < 4; ++c) dst[c] = to8(sampleAt(line, s + c, h.depth), h.depth);
                            break;
                    }
                }
            }
        }

        bool paletteIndicesValid(const std::vector<u8>& data, const Header& h, std::size_t stride, std::size_t paletteSize) {
            for (u32 y = 0; y < h.height; ++y) {
                const u8* line = data.data() + y * (stride + 1) + 1;
                for (u32 x = 0; x < h.width; ++x) {
                    if (sampleAt(line, x, h.depth) >= paletteSize) return false;
                }
            }
            return true;
        }
    }

    bool isPng(const u8* data, std::size_t size) {
        return size >= sizeof(SIGNATURE) && std::memcmp(data, SIGNATURE, sizeof(SIGNATURE)) == 0;
    }

    bool decodePng(const u8* data, std::size_t size, Image& out, std::string& error) {
        out = {};
        if (!isPng(data, size)) return fail(error, "not a PNG file");

        Header header;
        bool haveHeader = false, haveEnd = false;
        std::vector<u8> palette, paletteAlpha, compressed;
        Transparency trns;

        std::size_t position = sizeof(SIGNATURE);
        while (!haveEnd) {
            if (size - position < 12) return fail(error, "the file is cut short");
            const u32 length = readU32(data + position);
            const u8* type = data + position + 4;
            if (length > 0x7FFFFFFFu || size - position - 12 < length) return fail(error, "the file is cut short");
            const u8* body = type + 4;
            if (crc32(type, length + 4) != readU32(body + length)) return fail(error, "the file is damaged (a chunk's checksum is wrong)");
            position += 12 + static_cast<std::size_t>(length);

            const bool isHeader = std::memcmp(type, "IHDR", 4) == 0;
            if (!haveHeader && !isHeader) return fail(error, "the file is damaged (no header first)");

            if (isHeader) {
                if (haveHeader) return fail(error, "the file is damaged (two headers)");
                if (length != 13) return fail(error, "the file is damaged (bad header)");
                header.width = readU32(body);
                header.height = readU32(body + 4);
                header.depth = body[8];
                header.colorType = body[9];
                if (header.width == 0 || header.height == 0) return fail(error, "the image is empty");
                if (header.width > MAX_DIMENSION || header.height > MAX_DIMENSION) return fail(error, "the image is too large (16384 pixels a side at most)");
                if (!validDepth(header.colorType, header.depth)) return fail(error, "the file is damaged (bad color type or bit depth)");
                if (body[10] != 0 || body[11] != 0) return fail(error, "the file uses a compression or filter method PNG doesn't define");
                if (body[12] == 1) return fail(error, "interlaced PNGs aren't supported yet; save it without interlacing");
                if (body[12] != 0) return fail(error, "the file is damaged (bad interlace method)");
                haveHeader = true;
            } else if (std::memcmp(type, "PLTE", 4) == 0) {
                if (length % 3 != 0 || length == 0 || length > 256 * 3) return fail(error, "the file is damaged (bad palette)");
                palette.assign(body, body + length);
            } else if (std::memcmp(type, "tRNS", 4) == 0) {
                if (header.colorType == Palette) {
                    if (length > palette.size() / 3) return fail(error, "the file is damaged (bad transparency)");
                    paletteAlpha.assign(body, body + length);
                } else if (header.colorType == Grey) {
                    if (length != 2) return fail(error, "the file is damaged (bad transparency)");
                    trns.present = true;
                    trns.grey = readU16(body);
                } else if (header.colorType == Rgb) {
                    if (length != 6) return fail(error, "the file is damaged (bad transparency)");
                    trns.present = true;
                    for (int c = 0; c < 3; ++c) trns.rgb[c] = readU16(body + c * 2);
                }
                // Images with an alpha channel shouldn't carry tRNS; it's ignored
            } else if (std::memcmp(type, "IDAT", 4) == 0) {
                compressed.insert(compressed.end(), body, body + length);
            } else if (std::memcmp(type, "IEND", 4) == 0) {
                haveEnd = true;
            } else if ((type[0] & 0x20) == 0) {
                // A lowercase first letter marks a chunk that can be skipped; uppercase ones are needed to show the image
                return fail(error, "the file needs a feature this reader doesn't know");
            }
        }

        if (compressed.empty()) return fail(error, "the file has no image data");
        if (header.colorType == Palette && palette.empty()) return fail(error, "the file is damaged (no palette)");

        const int channels = channelsOf(header.colorType);
        const std::size_t bitsPerPixel = static_cast<std::size_t>(channels) * header.depth;
        const std::size_t stride = (header.width * bitsPerPixel + 7) / 8;
        const std::size_t bpp = std::max<std::size_t>(1, bitsPerPixel / 8);
        const std::size_t expected = (stride + 1) * header.height;

        std::vector<u8> raw;
        if (!inflateZlib(compressed.data(), compressed.size(), raw, error, expected)) return false;
        if (raw.size() != expected) return fail(error, "the image data is cut short");
        compressed = {};

        if (!unfilter(raw, header.height, stride, bpp, error)) return false;
        if (header.colorType == Palette && !paletteIndicesValid(raw, header, stride, palette.size() / 3)) {
            return fail(error, "the file is damaged (a pixel uses a color the palette doesn't have)");
        }

        toRgba(raw, header, stride, palette, paletteAlpha, trns, out.pixels);
        out.width = header.width;
        out.height = header.height;
        return true;
    }
}
