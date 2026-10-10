#include "core/io/crc32.hpp"

#include <array>

namespace {
    // Eight tables let the loop take eight bytes per step ("slicing by 8")
    using Tables = std::array<std::array<u32, 256>, 8>;

    constexpr Tables makeTables() {
        Tables tables {};

        for (u32 i = 0; i < 256; ++i) {
            u32 value = i;
            for (int bit = 0; bit < 8; ++bit) value = (value & 1) ? (value >> 1) ^ 0xEDB88320u : value >> 1;
            tables[0][i] = value;
        }

        for (u32 i = 0; i < 256; ++i) {
            for (std::size_t t = 1; t < 8; ++t) tables[t][i] = (tables[t - 1][i] >> 8) ^ tables[0][tables[t - 1][i] & 0xFF];
        }

        return tables;
    }

    constexpr Tables TABLES = makeTables();
}

u32 crc32(const void* data, std::size_t size) {
    const u8* bytes = static_cast<const u8*>(data);
    u32 crc = 0xFFFFFFFFu;

    while (size >= 8) {
        const u32 low = crc ^ (u32(bytes[0]) | u32(bytes[1]) << 8 | u32(bytes[2]) << 16 | u32(bytes[3]) << 24);
        crc = TABLES[7][low & 0xFF] ^ TABLES[6][(low >> 8) & 0xFF] ^ TABLES[5][(low >> 16) & 0xFF] ^ TABLES[4][low >> 24]
            ^ TABLES[3][bytes[4]] ^ TABLES[2][bytes[5]] ^ TABLES[1][bytes[6]] ^ TABLES[0][bytes[7]];
        bytes += 8;
        size -= 8;
    }

    while (size-- > 0) crc = (crc >> 8) ^ TABLES[0][(crc ^ *bytes++) & 0xFF];

    return crc ^ 0xFFFFFFFFu;
}
