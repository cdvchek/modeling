#include "image.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>

namespace image {
    namespace {
        constexpr u8 SIGNATURE[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
        constexpr std::size_t PIXEL_BYTES = 4;
        // Row filters: none, sub, up, average, Paeth
        constexpr u8 FILTERS = 5;

        u8 paeth(int a, int b, int c) {
            const int p = a + b - c;
            const int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
            return static_cast<u8>(pa <= pb && pa <= pc ? a : pb <= pc ? b : c);
        }

        // Filters a row against the one above; returns the sum of the results as distances from zero (smaller compresses better)
        std::size_t filterRow(u8 filter, const u8* row, const u8* above, std::size_t rowBytes, u8* out) {
            std::size_t sum = 0;
            for (std::size_t i = 0; i < rowBytes; ++i) {
                const int left = i >= PIXEL_BYTES ? row[i - PIXEL_BYTES] : 0;
                const int up = above[i];
                const int upLeft = i >= PIXEL_BYTES ? above[i - PIXEL_BYTES] : 0;
                int predicted = 0;
                switch (filter) {
                    case 1: predicted = left; break;
                    case 2: predicted = up; break;
                    case 3: predicted = (left + up) / 2; break;
                    case 4: predicted = paeth(left, up, upLeft); break;
                    default: break;
                }
                const u8 value = static_cast<u8>(row[i] - predicted);
                out[i] = value;
                sum += value < 128 ? value : 256 - value;
            }
            return sum;
        }

        std::array<u32, 256> makeCrcTable() {
            std::array<u32, 256> table {};
            for (u32 n = 0; n < 256; ++n) {
                u32 c = n;
                for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
                table[n] = c;
            }
            return table;
        }

        u32 crc(const u8* data, std::size_t size, u32 start = 0xFFFFFFFFu) {
            static const std::array<u32, 256> table = makeCrcTable();
            u32 c = start;
            for (std::size_t i = 0; i < size; ++i) c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
            return c;
        }

        void appendU32(std::vector<u8>& out, u32 value) {
            out.push_back(static_cast<u8>(value >> 24));
            out.push_back(static_cast<u8>(value >> 16));
            out.push_back(static_cast<u8>(value >> 8));
            out.push_back(static_cast<u8>(value));
        }

        void chunk(std::vector<u8>& out, const char type[4], const std::vector<u8>& data) {
            appendU32(out, static_cast<u32>(data.size()));
            const std::size_t start = out.size();
            out.insert(out.end(), type, type + 4);
            out.insert(out.end(), data.begin(), data.end());
            appendU32(out, crc(out.data() + start, out.size() - start) ^ 0xFFFFFFFFu);
        }
    }

    std::vector<u8> encodePng(const Image& image) {
        // Rows, each after the filter that leaves it the smallest values
        const std::size_t rowBytes = std::size_t(image.width) * PIXEL_BYTES;
        std::vector<u8> raw((rowBytes + 1) * image.height);
        const std::vector<u8> blank(rowBytes, 0);
        std::vector<u8> filtered(rowBytes);
        for (u32 y = 0; y < image.height; ++y) {
            const u8* row = image.pixels.data() + y * rowBytes;
            const u8* above = y > 0 ? row - rowBytes : blank.data();
            u8* out = raw.data() + y * (rowBytes + 1);

            std::size_t best = SIZE_MAX;
            for (u8 filter = 0; filter < FILTERS && best > 0; ++filter) {
                const std::size_t size = filterRow(filter, row, above, rowBytes, filtered.data());
                if (size >= best) continue;
                best = size;
                out[0] = filter;
                std::copy(filtered.begin(), filtered.end(), out + 1);
            }
        }

        const std::vector<u8> zlib = deflateZlib(raw.data(), raw.size());

        std::vector<u8> out(std::begin(SIGNATURE), std::end(SIGNATURE));
        std::vector<u8> header;
        appendU32(header, image.width);
        appendU32(header, image.height);
        header.insert(header.end(), { 8, 6, 0, 0, 0 });   // 8 bits, RGBA, deflate, adaptive filtering, not interlaced
        chunk(out, "IHDR", header);
        chunk(out, "IDAT", zlib);
        chunk(out, "IEND", {});
        return out;
    }
}
