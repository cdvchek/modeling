#include "image/image.hpp"

#include <algorithm>
#include <array>
#include <bit>

// DEFLATE compression (RFC 1951): hash-chain matching with one step of lazy evaluation, then stored, fixed, or dynamic blocks

namespace image {
    namespace {
        constexpr int LITERAL_CODES = 286;
        constexpr int DISTANCE_CODES = 30;
        constexpr int LENGTH_CODES = 19;
        constexpr int END_OF_BLOCK = 256;
        constexpr int MAX_BITS = 15;
        constexpr int MAX_LENGTH_BITS = 7;

        constexpr std::size_t WINDOW = 32768;
        constexpr std::size_t MIN_MATCH = 3;
        constexpr std::size_t MAX_MATCH = 258;
        constexpr std::size_t MAX_STORED = 65535;
        constexpr std::size_t BLOCK_TOKENS = 65536;
        constexpr u32 HASH_BITS = 15;
        constexpr u32 NONE = 0xFFFFFFFFu;
        // How hard to look: candidates tried per position, a length good enough to stop at, and one not worth deferring
        constexpr int MAX_CHAIN = 48;
        constexpr std::size_t NICE_MATCH = 128;
        constexpr std::size_t LAZY_MATCH = 32;
        // A minimum-length match this far back costs more than its three literals
        constexpr std::size_t FAR_SHORT_MATCH = 4096;

        constexpr u32 MATCH_TOKEN = 0x80000000u;
        // The order code-length code lengths are written in
        constexpr int LENGTH_ORDER[LENGTH_CODES] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

        struct BitWriter {
            std::vector<u8>& out;
            std::uint64_t buffer = 0;
            int count = 0;

            // count bits of value, least significant first
            void put(u32 value, int bits) {
                buffer |= static_cast<std::uint64_t>(value) << count;
                count += bits;
                while (count >= 8) {
                    out.push_back(static_cast<u8>(buffer));
                    buffer >>= 8;
                    count -= 8;
                }
            }

            void alignToByte() {
                if (count > 0) put(0, 8 - count);
            }
        };

        // A symbol of a length or distance code, with the extra bits that follow it
        struct Coded {
            int symbol;
            int extraBits;
            u32 extra;
        };

        Coded lengthCode(std::size_t length) {
            const u32 l = static_cast<u32>(length - MIN_MATCH);
            if (l < 8) return { 257 + static_cast<int>(l), 0, 0 };
            if (length == MAX_MATCH) return { 285, 0, 0 };
            const int high = std::bit_width(l) - 1;
            return { 257 + 4 * (high - 1) + static_cast<int>((l >> (high - 2)) & 3), high - 2, l & ((1u << (high - 2)) - 1u) };
        }

        Coded distanceCode(std::size_t distance) {
            const u32 d = static_cast<u32>(distance - 1);
            if (d < 4) return { static_cast<int>(d), 0, 0 };
            const int high = std::bit_width(d) - 1;
            return { 2 * high + static_cast<int>((d >> (high - 1)) & 1), high - 1, d & ((1u << (high - 1)) - 1u) };
        }

        // Code lengths for the symbols' frequencies, none longer than maxBits; unused symbols get 0
        void codeLengths(const u32* frequency, int count, int maxBits, u8* lengths) {
            std::fill(lengths, lengths + count, u8(0));
            std::vector<int> used;
            for (int s = 0; s < count; ++s) {
                if (frequency[s] > 0) used.push_back(s);
            }
            const int n = static_cast<int>(used.size());
            if (n == 0) return;
            if (n == 1) {
                lengths[used[0]] = 1;
                return;
            }
            std::stable_sort(used.begin(), used.end(), [&](int a, int b) { return frequency[a] < frequency[b]; });

            // Huffman tree: leaves first in rising frequency, then each joined pair, which also come out rising
            std::vector<std::uint64_t> weight(2 * n - 1);
            std::vector<int> parent(2 * n - 1, 0);
            for (int i = 0; i < n; ++i) weight[i] = frequency[used[i]];
            int leaf = 0, joined = n;
            for (int next = n; next < 2 * n - 1; ++next) {
                std::uint64_t sum = 0;
                for (int k = 0; k < 2; ++k) {
                    const bool takeLeaf = leaf < n && (joined >= next || weight[leaf] <= weight[joined]);
                    const int node = takeLeaf ? leaf++ : joined++;
                    parent[node] = next;
                    sum += weight[node];
                }
                weight[next] = sum;
            }

            // How many leaves sit at each depth
            std::vector<int> depth(2 * n - 1, 0);
            std::vector<int> perLength(std::max(n, maxBits + 1) + 1, 0);
            for (int i = 2 * n - 3; i >= 0; --i) {
                depth[i] = depth[parent[i]] + 1;
                if (i < n) perLength[depth[i]]++;
            }

            // Codes past the limit move up to it, then others move down until the code space fits again
            for (std::size_t i = maxBits + 1; i < perLength.size(); ++i) perLength[maxBits] += perLength[i];
            u32 total = 0;
            for (int i = maxBits; i > 0; --i) total += static_cast<u32>(perLength[i]) << (maxBits - i);
            while (total != (1u << maxBits)) {
                perLength[maxBits]--;
                for (int i = maxBits - 1; i > 0; --i) {
                    if (perLength[i] == 0) continue;
                    perLength[i]--;
                    perLength[i + 1] += 2;
                    break;
                }
                total--;
            }

            // The most frequent symbols take the shortest codes
            int at = n;
            for (int length = 1; length <= maxBits; ++length) {
                for (int k = 0; k < perLength[length]; ++k) lengths[used[--at]] = static_cast<u8>(length);
            }
        }

        // Canonical codes for the lengths, bit-reversed so they can be written least significant bit first
        void canonicalCodes(const u8* lengths, int count, u32* codes) {
            std::array<u32, MAX_BITS + 2> next {};
            for (int s = 0; s < count; ++s) next[lengths[s] + 1]++;
            next[1] = 0;
            for (int length = 1; length <= MAX_BITS; ++length) next[length + 1] = (next[length + 1] + next[length]) << 1;
            for (int s = 0; s < count; ++s) {
                if (lengths[s] == 0) continue;
                u32 code = next[lengths[s]]++;
                u32 reversed = 0;
                for (int bit = 0; bit < lengths[s]; ++bit, code >>= 1) reversed = (reversed << 1) | (code & 1);
                codes[s] = reversed;
            }
        }

        struct Code {
            std::array<u8, LITERAL_CODES + 2> literalLengths {};
            std::array<u32, LITERAL_CODES + 2> literalCodes {};
            std::array<u8, DISTANCE_CODES> distanceLengths {};
            std::array<u32, DISTANCE_CODES> distanceCodes {};

            void assign() {
                canonicalCodes(literalLengths.data(), static_cast<int>(literalLengths.size()), literalCodes.data());
                canonicalCodes(distanceLengths.data(), DISTANCE_CODES, distanceCodes.data());
            }
        };

        // The code every decoder knows without a header
        const Code& fixedCode() {
            static const Code code = [] {
                Code c;
                for (int s = 0; s < 288; ++s) c.literalLengths[s] = s < 144 ? 8 : s < 256 ? 9 : s < 280 ? 7 : 8;
                c.distanceLengths.fill(5);
                c.assign();
                return c;
            }();
            return code;
        }

        // One entry of a dynamic block's header: a code-length symbol and its repeat count
        struct LengthItem {
            u8 symbol;
            u8 extraBits;
            u8 extra;
        };

        // A dynamic block's header: both codes' lengths, run-length coded, and the code those are written in
        struct DynamicHeader {
            int literalCount = 257;
            int distanceCount = 1;
            int lengthCount = 4;
            std::vector<LengthItem> items;
            std::array<u8, LENGTH_CODES> lengths {};
            std::array<u32, LENGTH_CODES> codes {};
            std::size_t bits = 0;
        };

        DynamicHeader makeHeader(const Code& code) {
            DynamicHeader header;
            header.literalCount = LITERAL_CODES;
            while (header.literalCount > 257 && code.literalLengths[header.literalCount - 1] == 0) header.literalCount--;
            header.distanceCount = DISTANCE_CODES;
            while (header.distanceCount > 1 && code.distanceLengths[header.distanceCount - 1] == 0) header.distanceCount--;

            std::vector<u8> all(code.literalLengths.begin(), code.literalLengths.begin() + header.literalCount);
            all.insert(all.end(), code.distanceLengths.begin(), code.distanceLengths.begin() + header.distanceCount);

            // Runs of one length become repeat symbols: 16 repeats the last length, 17 and 18 repeat zero
            for (std::size_t i = 0; i < all.size();) {
                const u8 value = all[i];
                std::size_t run = 1;
                while (i + run < all.size() && all[i + run] == value) run++;
                i += run;
                if (value != 0) {
                    header.items.push_back({ value, 0, 0 });
                    run--;
                }
                while (run > 0) {
                    std::size_t take = 1;
                    if (value == 0 && run >= 11) {
                        take = std::min<std::size_t>(run, 138);
                        header.items.push_back({ 18, 7, static_cast<u8>(take - 11) });
                    } else if (value == 0 && run >= 3) {
                        take = run;
                        header.items.push_back({ 17, 3, static_cast<u8>(take - 3) });
                    } else if (value != 0 && run >= 3) {
                        take = std::min<std::size_t>(run, 6);
                        header.items.push_back({ 16, 2, static_cast<u8>(take - 3) });
                    } else {
                        header.items.push_back({ value, 0, 0 });
                    }
                    run -= take;
                }
            }

            std::array<u32, LENGTH_CODES> frequency {};
            for (const LengthItem& item : header.items) frequency[item.symbol]++;
            // A code needs two symbols to be complete
            if (std::count_if(frequency.begin(), frequency.end(), [](u32 f) { return f > 0; }) < 2) {
                frequency[frequency[0] == 0 ? 0 : 18]++;
            }
            codeLengths(frequency.data(), LENGTH_CODES, MAX_LENGTH_BITS, header.lengths.data());
            canonicalCodes(header.lengths.data(), LENGTH_CODES, header.codes.data());

            header.lengthCount = LENGTH_CODES;
            while (header.lengthCount > 4 && header.lengths[LENGTH_ORDER[header.lengthCount - 1]] == 0) header.lengthCount--;
            header.bits = 5 + 5 + 4 + 3 * static_cast<std::size_t>(header.lengthCount);
            for (const LengthItem& item : header.items) header.bits += header.lengths[item.symbol] + item.extraBits;
            return header;
        }

        class Deflater {
        public:
            Deflater(const u8* data, std::size_t size, std::vector<u8>& out)
                : m_data(data), m_size(size), m_writer { out }, m_head(std::size_t(1) << HASH_BITS, NONE), m_previous(WINDOW, NONE) {
                m_tokens.reserve(BLOCK_TOKENS);
            }

            void run() {
                std::size_t position = 0;
                Match current = findAndInsert(position);
                while (position < m_size) {
                    if (m_tokens.size() == BLOCK_TOKENS) flush(position, false);

                    if (current.length < MIN_MATCH) {
                        literal(m_data[position]);
                        position++;
                        current = findAndInsert(position);
                        continue;
                    }

                    // A longer match starting one byte later wins: this byte goes out as a literal
                    if (current.length < LAZY_MATCH && position + 1 < m_size) {
                        const Match next = findAndInsert(position + 1);
                        if (next.length > current.length) {
                            literal(m_data[position]);
                            position++;
                            current = next;
                            continue;
                        }
                        insertRange(position + 2, position + current.length);
                    } else {
                        insertRange(position + 1, position + current.length);
                    }
                    match(current);
                    position += current.length;
                    current = findAndInsert(position);
                }
                flush(m_size, true);
                m_writer.alignToByte();
            }

        private:
            struct Match {
                std::size_t length = 0;
                std::size_t distance = 0;
            };

            u32 hash(std::size_t position) const {
                const u32 three = m_data[position] | (u32(m_data[position + 1]) << 8) | (u32(m_data[position + 2]) << 16);
                return (three * 0x9E3779B1u) >> (32 - HASH_BITS);
            }

            void insert(std::size_t position) {
                if (position + MIN_MATCH > m_size) return;
                const u32 h = hash(position);
                m_previous[position & (WINDOW - 1)] = m_head[h];
                m_head[h] = static_cast<u32>(position);
            }

            void insertRange(std::size_t from, std::size_t to) {
                for (std::size_t position = from; position < to; ++position) insert(position);
            }

            // The longest earlier copy of what starts at position, which then joins the chains itself
            Match findAndInsert(std::size_t position) {
                Match best;
                if (position + MIN_MATCH > m_size) return best;
                const std::size_t limit = std::min(MAX_MATCH, m_size - position);
                const u8* here = m_data + position;

                u32 candidate = m_head[hash(position)];
                for (int chain = MAX_CHAIN; chain > 0 && candidate != NONE && position - candidate <= WINDOW; --chain) {
                    const u8* there = m_data + candidate;
                    if (best.length == 0 || there[best.length] == here[best.length]) {
                        std::size_t length = 0;
                        while (length < limit && there[length] == here[length]) length++;
                        if (length > best.length) {
                            best = { length, position - candidate };
                            if (length >= NICE_MATCH || length == limit) break;
                        }
                    }
                    // A slot reused by a newer position ends the chain
                    const u32 older = m_previous[candidate & (WINDOW - 1)];
                    if (older != NONE && older >= candidate) break;
                    candidate = older;
                }
                insert(position);

                if (best.length < MIN_MATCH || (best.length == MIN_MATCH && best.distance > FAR_SHORT_MATCH)) return {};
                return best;
            }

            void literal(u8 byte) {
                m_tokens.push_back(byte);
                m_literalFrequency[byte]++;
            }

            void match(const Match& found) {
                m_tokens.push_back(MATCH_TOKEN | (static_cast<u32>(found.length) << 16) | static_cast<u32>(found.distance - 1));
                const Coded length = lengthCode(found.length);
                const Coded distance = distanceCode(found.distance);
                m_literalFrequency[length.symbol]++;
                m_distanceFrequency[distance.symbol]++;
                m_extraBits += length.extraBits + distance.extraBits;
            }

            // Bits the block's symbols take in a code, without its header
            std::size_t cost(const Code& code) const {
                std::size_t bits = m_extraBits;
                for (int s = 0; s < LITERAL_CODES; ++s) bits += std::size_t(m_literalFrequency[s]) * code.literalLengths[s];
                for (int s = 0; s < DISTANCE_CODES; ++s) bits += std::size_t(m_distanceFrequency[s]) * code.distanceLengths[s];
                return bits;
            }

            // Writes the tokens gathered since the last block, covering the input up to end, in whichever form is smallest
            void flush(std::size_t end, bool last) {
                m_literalFrequency[END_OF_BLOCK]++;

                // Two distance symbols at least, so the code is complete for every decoder
                std::array<u32, DISTANCE_CODES> distanceFrequency = m_distanceFrequency;
                for (int s = 0; s < 2; ++s) distanceFrequency[s] = std::max(distanceFrequency[s], 1u);
                Code dynamic;
                codeLengths(m_literalFrequency.data(), LITERAL_CODES, MAX_BITS, dynamic.literalLengths.data());
                codeLengths(distanceFrequency.data(), DISTANCE_CODES, MAX_BITS, dynamic.distanceLengths.data());
                dynamic.assign();
                const DynamicHeader header = makeHeader(dynamic);

                const std::size_t bytes = end - m_blockStart;
                const std::size_t storedBlocks = std::max<std::size_t>(1, (bytes + MAX_STORED - 1) / MAX_STORED);
                const std::size_t storedBits = bytes * 8 + storedBlocks * (3 + 7 + 32);
                const std::size_t fixedBits = 3 + cost(fixedCode());
                const std::size_t dynamicBits = 3 + header.bits + cost(dynamic);

                if (storedBits <= fixedBits && storedBits <= dynamicBits) {
                    writeStored(end, last);
                } else if (fixedBits <= dynamicBits) {
                    m_writer.put(last ? 1 : 0, 1);
                    m_writer.put(1, 2);
                    writeTokens(fixedCode());
                } else {
                    m_writer.put(last ? 1 : 0, 1);
                    m_writer.put(2, 2);
                    writeHeader(header);
                    writeTokens(dynamic);
                }

                m_tokens.clear();
                m_literalFrequency.fill(0);
                m_distanceFrequency.fill(0);
                m_extraBits = 0;
                m_blockStart = end;
            }

            void writeStored(std::size_t end, bool last) {
                std::size_t at = m_blockStart;
                do {
                    const std::size_t length = std::min(MAX_STORED, end - at);
                    m_writer.put(last && at + length == end ? 1 : 0, 1);
                    m_writer.put(0, 2);
                    m_writer.alignToByte();
                    m_writer.put(static_cast<u32>(length), 16);
                    m_writer.put(static_cast<u32>(~length & 0xFFFFu), 16);
                    m_writer.out.insert(m_writer.out.end(), m_data + at, m_data + at + length);
                    at += length;
                } while (at < end);
            }

            void writeHeader(const DynamicHeader& header) {
                m_writer.put(static_cast<u32>(header.literalCount - 257), 5);
                m_writer.put(static_cast<u32>(header.distanceCount - 1), 5);
                m_writer.put(static_cast<u32>(header.lengthCount - 4), 4);
                for (int i = 0; i < header.lengthCount; ++i) m_writer.put(header.lengths[LENGTH_ORDER[i]], 3);
                for (const LengthItem& item : header.items) {
                    m_writer.put(header.codes[item.symbol], header.lengths[item.symbol]);
                    if (item.extraBits > 0) m_writer.put(item.extra, item.extraBits);
                }
            }

            void writeTokens(const Code& code) {
                for (const u32 token : m_tokens) {
                    if ((token & MATCH_TOKEN) == 0) {
                        m_writer.put(code.literalCodes[token], code.literalLengths[token]);
                        continue;
                    }
                    const Coded length = lengthCode((token >> 16) & 0x1FF);
                    const Coded distance = distanceCode((token & 0x7FFF) + 1);
                    m_writer.put(code.literalCodes[length.symbol], code.literalLengths[length.symbol]);
                    if (length.extraBits > 0) m_writer.put(length.extra, length.extraBits);
                    m_writer.put(code.distanceCodes[distance.symbol], code.distanceLengths[distance.symbol]);
                    if (distance.extraBits > 0) m_writer.put(distance.extra, distance.extraBits);
                }
                m_writer.put(code.literalCodes[END_OF_BLOCK], code.literalLengths[END_OF_BLOCK]);
            }

            const u8* m_data;
            std::size_t m_size;
            BitWriter m_writer;
            // The latest position with each hash, and for each position in the window the one before it with the same hash
            std::vector<u32> m_head;
            std::vector<u32> m_previous;

            // The block being gathered: literals as their byte, matches as MATCH_TOKEN, length, and distance - 1
            std::vector<u32> m_tokens;
            std::array<u32, LITERAL_CODES> m_literalFrequency {};
            std::array<u32, DISTANCE_CODES> m_distanceFrequency {};
            std::size_t m_extraBits = 0;
            std::size_t m_blockStart = 0;
        };

        u32 adler32(const u8* data, std::size_t size) {
            // The sums can't overflow within this many bytes, so the remainder is taken once per run
            constexpr std::size_t RUN = 5552;
            u32 a = 1, b = 0;
            for (std::size_t at = 0; at < size;) {
                const std::size_t end = std::min(size, at + RUN);
                for (; at < end; ++at) {
                    a += data[at];
                    b += a;
                }
                a %= 65521;
                b %= 65521;
            }
            return (b << 16) | a;
        }
    }

    std::vector<u8> deflateZlib(const u8* data, std::size_t size) {
        std::vector<u8> out = { 0x78, 0x9C };
        out.reserve(size / 4 + 64);
        Deflater(data, size, out).run();

        const u32 check = adler32(data, size);
        for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<u8>(check >> shift));
        return out;
    }
}
