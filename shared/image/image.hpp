#pragma once

// Image files for the suite (Valuma, and later the Aevora engine). Depends only on the C++ standard library.
// PNG for now; JPEG and interlaced PNGs are planned.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace image {
    using u8 = std::uint8_t;
    using u32 = std::uint32_t;

    // 8-bit RGBA, rows from the top, no padding: pixels.size() == width * height * 4
    struct Image {
        u32 width = 0;
        u32 height = 0;
        std::vector<u8> pixels;
    };

    // Images wider or taller than this are refused (GPUs can't hold more than this in one texture anyway)
    inline constexpr u32 MAX_DIMENSION = 16384;

    // Any PNG that isn't interlaced: grey, RGB, palette (with transparency), grey + alpha, RGBA, at every bit depth.
    // Every format becomes 8-bit RGBA; 16-bit samples keep their high byte. On failure error says why.
    bool decodePng(const u8* data, std::size_t size, Image& out, std::string& error);

    // True if the bytes start with the PNG signature
    bool isPng(const u8* data, std::size_t size);

    // A PNG of the image: 8-bit RGBA, not interlaced, compressed, with a filter picked for each row
    std::vector<u8> encodePng(const Image& image);

    // Compresses to zlib-wrapped DEFLATE, in whichever of stored, fixed, and dynamic Huffman blocks comes out smallest
    std::vector<u8> deflateZlib(const u8* data, std::size_t size);

    // zlib-wrapped DEFLATE (RFC 1950/1951): stored, fixed, and dynamic Huffman blocks, with the Adler-32 check.
    // The output is allocated once at maxSize; output past it fails.
    bool inflateZlib(const u8* data, std::size_t size, std::vector<u8>& out, std::string& error, std::size_t maxSize);
}
