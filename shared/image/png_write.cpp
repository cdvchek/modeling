#include "image.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace image {
    namespace {
        constexpr u8 SIGNATURE[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
        // A stored (uncompressed) DEFLATE block holds at most this many bytes
        constexpr std::size_t MAX_STORED = 65535;

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
        // Rows, each after filter byte 0 (none)
        const std::size_t rowBytes = std::size_t(image.width) * 4;
        std::vector<u8> raw;
        raw.reserve((rowBytes + 1) * image.height);
        for (u32 y = 0; y < image.height; ++y) {
            raw.push_back(0);
            raw.insert(raw.end(), image.pixels.begin() + y * rowBytes, image.pixels.begin() + (y + 1) * rowBytes);
        }

        // zlib: header, stored blocks, Adler-32 of the raw bytes
        std::vector<u8> zlib = { 0x78, 0x01 };
        for (std::size_t at = 0; at < raw.size() || at == 0; at += MAX_STORED) {
            const std::size_t length = std::min(MAX_STORED, raw.size() - at);
            const bool last = at + length >= raw.size();
            zlib.push_back(last ? 1 : 0);
            zlib.push_back(static_cast<u8>(length));
            zlib.push_back(static_cast<u8>(length >> 8));
            zlib.push_back(static_cast<u8>(~length));
            zlib.push_back(static_cast<u8>(~length >> 8));
            zlib.insert(zlib.end(), raw.begin() + at, raw.begin() + at + length);
            if (raw.empty()) break;
        }
        u32 a = 1, b = 0;
        for (u8 byte : raw) {
            a = (a + byte) % 65521;
            b = (b + a) % 65521;
        }
        appendU32(zlib, (b << 16) | a);

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
