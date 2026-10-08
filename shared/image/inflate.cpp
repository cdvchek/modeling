#include "image/image.hpp"

#include <algorithm>
#include <array>

// DEFLATE as RFC 1951 describes it. Huffman codes are decoded canonically, a bit at a time against the count of
// codes of each length (as zlib's "puff" does): short and easy to check, and fast enough for reference images.

namespace image {
    namespace {
        constexpr int MAX_BITS = 15;
        constexpr int MAX_LITERAL_CODES = 288;
        constexpr int MAX_DISTANCE_CODES = 30;

        struct BitReader {
            const u8* data;
            std::size_t size;
            std::size_t position = 0;
            u32 bitBuffer = 0;
            int bitCount = 0;
            bool overrun = false;

            // need bits, least significant first; reading past the end sets overrun and gives zeros
            u32 bits(int need) {
                u32 value = bitBuffer;
                while (bitCount < need) {
                    if (position >= size) {
                        overrun = true;
                        return 0;
                    }
                    value |= static_cast<u32>(data[position++]) << bitCount;
                    bitCount += 8;
                }
                bitBuffer = value >> need;
                bitCount -= need;
                return value & ((1u << need) - 1u);
            }

            void alignToByte() {
                bitBuffer = 0;
                bitCount = 0;
            }
        };

        // A canonical Huffman code: how many codes have each length, and the symbols in code order
        struct Huffman {
            std::array<short, MAX_BITS + 1> count {};
            std::array<short, MAX_LITERAL_CODES> symbol {};
        };

        // Builds the code from each symbol's code length; false if the lengths over-subscribe the code space
        bool build(Huffman& h, const short* lengths, int n) {
            h.count.fill(0);
            for (int s = 0; s < n; ++s) h.count[lengths[s]]++;
            if (h.count[0] == n) return true;   // no codes: fine as long as it's never used

            int left = 1;
            for (int len = 1; len <= MAX_BITS; ++len) {
                left <<= 1;
                left -= h.count[len];
                if (left < 0) return false;
            }

            std::array<short, MAX_BITS + 1> offsets {};
            for (int len = 1; len < MAX_BITS; ++len) offsets[len + 1] = offsets[len] + h.count[len];
            for (int s = 0; s < n; ++s) {
                if (lengths[s] != 0) h.symbol[offsets[lengths[s]]++] = static_cast<short>(s);
            }
            return true;
        }

        // The next symbol, or -1 for a bit pattern no code uses
        int decode(BitReader& in, const Huffman& h) {
            int code = 0, first = 0, index = 0;
            for (int len = 1; len <= MAX_BITS; ++len) {
                code |= static_cast<int>(in.bits(1));
                const int count = h.count[len];
                if (code - count < first) return h.symbol[index + (code - first)];
                index += count;
                first += count;
                first <<= 1;
                code <<= 1;
                if (in.overrun) return -1;
            }
            return -1;
        }

        constexpr std::array<short, 29> LENGTH_BASE = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
        constexpr std::array<short, 29> LENGTH_EXTRA = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
        constexpr std::array<int, 30> DISTANCE_BASE = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
        constexpr std::array<short, 30> DISTANCE_EXTRA = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

        // Literal bytes and back-references until the end-of-block code
        bool inflateCodes(BitReader& in, std::vector<u8>& out, std::size_t maxSize, const Huffman& literals, const Huffman& distances, std::string& error) {
            for (;;) {
                const int symbol = decode(in, literals);
                if (symbol < 0) { error = "the image data is damaged (bad code)"; return false; }
                if (symbol < 256) {
                    if (out.size() >= maxSize) { error = "the image data is larger than its size says"; return false; }
                    out.push_back(static_cast<u8>(symbol));
                    continue;
                }
                if (symbol == 256) return true;

                const int lengthIndex = symbol - 257;
                if (lengthIndex >= 29) { error = "the image data is damaged (bad length)"; return false; }
                const std::size_t length = static_cast<std::size_t>(LENGTH_BASE[lengthIndex]) + in.bits(LENGTH_EXTRA[lengthIndex]);

                const int distanceIndex = decode(in, distances);
                if (distanceIndex < 0 || distanceIndex >= 30) { error = "the image data is damaged (bad distance)"; return false; }
                const std::size_t distance = static_cast<std::size_t>(DISTANCE_BASE[distanceIndex]) + in.bits(DISTANCE_EXTRA[distanceIndex]);

                if (in.overrun) { error = "the image data is cut short"; return false; }
                if (distance > out.size()) { error = "the image data is damaged (reaches back too far)"; return false; }
                if (out.size() + length > maxSize) { error = "the image data is larger than its size says"; return false; }

                // Byte by byte, since a copy may overlap what it's writing (a run)
                const std::size_t from = out.size() - distance;
                for (std::size_t k = 0; k < length; ++k) out.push_back(out[from + k]);
            }
        }

        bool inflateStored(BitReader& in, std::vector<u8>& out, std::size_t maxSize, std::string& error) {
            in.alignToByte();
            if (in.size - in.position < 4) { error = "the image data is cut short"; return false; }
            const u32 length = in.data[in.position] | (in.data[in.position + 1] << 8);
            const u32 check = in.data[in.position + 2] | (in.data[in.position + 3] << 8);
            in.position += 4;
            if (length != (~check & 0xFFFFu)) { error = "the image data is damaged (stored block)"; return false; }
            if (in.size - in.position < length) { error = "the image data is cut short"; return false; }
            if (out.size() + length > maxSize) { error = "the image data is larger than its size says"; return false; }
            out.insert(out.end(), in.data + in.position, in.data + in.position + length);
            in.position += length;
            return true;
        }

        struct FixedCodes {
            Huffman literals, distances;

            FixedCodes() {
                std::array<short, MAX_LITERAL_CODES> lengths {};
                int s = 0;
                for (; s < 144; ++s) lengths[s] = 8;
                for (; s < 256; ++s) lengths[s] = 9;
                for (; s < 280; ++s) lengths[s] = 7;
                for (; s < MAX_LITERAL_CODES; ++s) lengths[s] = 8;
                build(literals, lengths.data(), MAX_LITERAL_CODES);

                std::array<short, MAX_DISTANCE_CODES> distanceLengths {};
                distanceLengths.fill(5);
                build(distances, distanceLengths.data(), MAX_DISTANCE_CODES);
            }
        };

        bool inflateFixed(BitReader& in, std::vector<u8>& out, std::size_t maxSize, std::string& error) {
            // Built once; a function-local static is safe when several threads decode at once
            static const FixedCodes fixed;
            return inflateCodes(in, out, maxSize, fixed.literals, fixed.distances, error);
        }

        bool inflateDynamic(BitReader& in, std::vector<u8>& out, std::size_t maxSize, std::string& error) {
            static constexpr std::array<int, 19> ORDER = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

            const int literalCount = static_cast<int>(in.bits(5)) + 257;
            const int distanceCount = static_cast<int>(in.bits(5)) + 1;
            const int lengthCodeCount = static_cast<int>(in.bits(4)) + 4;
            if (literalCount > 286 || distanceCount > 30) { error = "the image data is damaged (bad code counts)"; return false; }

            std::array<short, MAX_LITERAL_CODES + MAX_DISTANCE_CODES> lengths {};
            for (int i = 0; i < lengthCodeCount; ++i) lengths[ORDER[i]] = static_cast<short>(in.bits(3));

            Huffman lengthCode;
            if (!build(lengthCode, lengths.data(), 19)) { error = "the image data is damaged (bad code lengths)"; return false; }

            // The literal and distance code lengths, with runs
            int index = 0;
            while (index < literalCount + distanceCount) {
                const int symbol = decode(in, lengthCode);
                if (symbol < 0) { error = "the image data is damaged (bad code lengths)"; return false; }
                if (symbol < 16) {
                    lengths[index++] = static_cast<short>(symbol);
                    continue;
                }

                short repeated = 0;
                int times = 0;
                if (symbol == 16) {
                    if (index == 0) { error = "the image data is damaged (repeat with nothing before it)"; return false; }
                    repeated = lengths[index - 1];
                    times = 3 + static_cast<int>(in.bits(2));
                } else if (symbol == 17) {
                    times = 3 + static_cast<int>(in.bits(3));
                } else {
                    times = 11 + static_cast<int>(in.bits(7));
                }
                if (index + times > literalCount + distanceCount) { error = "the image data is damaged (too many code lengths)"; return false; }
                while (times-- > 0) lengths[index++] = repeated;
            }
            if (lengths[256] == 0) { error = "the image data is damaged (no end code)"; return false; }

            Huffman literals, distances;
            if (!build(literals, lengths.data(), literalCount) || !build(distances, lengths.data() + literalCount, distanceCount)) {
                error = "the image data is damaged (bad codes)";
                return false;
            }
            return inflateCodes(in, out, maxSize, literals, distances, error);
        }

        u32 adler32(const std::vector<u8>& data) {
            u32 a = 1, b = 0;
            std::size_t i = 0;
            while (i < data.size()) {
                // 5552 bytes is the most that can be summed before the 32-bit sums could overflow
                const std::size_t end = std::min(data.size(), i + 5552);
                for (; i < end; ++i) {
                    a += data[i];
                    b += a;
                }
                a %= 65521u;
                b %= 65521u;
            }
            return (b << 16) | a;
        }
    }

    bool inflateZlib(const u8* data, std::size_t size, std::vector<u8>& out, std::string& error, std::size_t maxSize) {
        // zlib header: deflate with a window up to 32 KB, no preset dictionary, and a check that the header is whole
        if (size < 6) { error = "the image data is cut short"; return false; }
        const u32 cmf = data[0], flags = data[1];
        if ((cmf & 0x0F) != 8 || (cmf >> 4) > 7 || ((cmf << 8) | flags) % 31 != 0 || (flags & 0x20)) {
            error = "the image data isn't compressed the way PNG requires";
            return false;
        }

        BitReader in { data + 2, size - 2 };
        out.clear();
        out.reserve(maxSize);

        bool last = false;
        while (!last) {
            last = in.bits(1) != 0;
            const u32 type = in.bits(2);
            if (in.overrun) { error = "the image data is cut short"; return false; }

            bool ok = false;
            if (type == 0) ok = inflateStored(in, out, maxSize, error);
            else if (type == 1) ok = inflateFixed(in, out, maxSize, error);
            else if (type == 2) ok = inflateDynamic(in, out, maxSize, error);
            else error = "the image data is damaged (bad block type)";
            if (!ok) return false;
        }

        // The Adler-32 of the output follows, byte-aligned, big-endian
        in.alignToByte();
        if (in.size - in.position < 4) { error = "the image data is cut short"; return false; }
        const u8* tail = in.data + in.position;
        const u32 expected = (u32(tail[0]) << 24) | (u32(tail[1]) << 16) | (u32(tail[2]) << 8) | u32(tail[3]);
        if (adler32(out) != expected) { error = "the image data is damaged (checksum)"; return false; }
        return true;
    }
}
