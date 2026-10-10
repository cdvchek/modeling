#pragma once

#include <cstddef>
#include <types>

// Standard CRC-32 (the zlib/PNG one), used to catch damaged file sections
u32 crc32(const void* data, std::size_t size);
