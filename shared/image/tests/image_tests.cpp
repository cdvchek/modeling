#include "test.hpp"
#include "image/image.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

// Fixtures come from make_fixtures.py: each <name>.png has a <name>.rgba (width, height, then the pixels) to match
namespace {
    using namespace image;

    std::vector<u8> readBytes(const std::string& name) {
        const auto path = std::filesystem::path(SOURCE_DIR) / "shared/image/tests/fixtures" / name;
        std::ifstream file(path, std::ios::binary);
        return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
    }

    bool decode(const std::vector<u8>& bytes, Image& out, std::string& error) {
        return decodePng(bytes.data(), bytes.size(), out, error);
    }

    // Decodes <name>.png and compares it with <name>.rgba
    bool matchesExpected(const std::string& name) {
        Image decoded;
        std::string error;
        if (!decode(readBytes(name + ".png"), decoded, error)) return false;

        const std::vector<u8> expected = readBytes(name + ".rgba");
        if (expected.size() < 8) return false;
        u32 width = 0, height = 0;
        std::memcpy(&width, expected.data(), 4);
        std::memcpy(&height, expected.data() + 4, 4);
        return decoded.width == width && decoded.height == height
            && decoded.pixels.size() == expected.size() - 8
            && std::memcmp(decoded.pixels.data(), expected.data() + 8, decoded.pixels.size()) == 0;
    }

    u32 crc(const u8* data, std::size_t size) {
        u32 c = 0xFFFFFFFFu;
        for (std::size_t i = 0; i < size; ++i) {
            c ^= data[i];
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        }
        return c ^ 0xFFFFFFFFu;
    }

    void putU32(u8* p, u32 v) {
        p[0] = static_cast<u8>(v >> 24);
        p[1] = static_cast<u8>(v >> 16);
        p[2] = static_cast<u8>(v >> 8);
        p[3] = static_cast<u8>(v);
    }

    // Changes a byte of the header chunk and fixes its CRC, so only the change itself is wrong
    std::vector<u8> withHeaderByte(std::vector<u8> png, std::size_t offset, u8 value) {
        png[16 + offset] = value;
        putU32(png.data() + 29, crc(png.data() + 12, 17));
        return png;
    }
}

TEST_CASE(png_decodes_every_grey_depth) {
    CHECK(matchesExpected("grey1"));
    CHECK(matchesExpected("grey2"));
    CHECK(matchesExpected("grey4"));
    CHECK(matchesExpected("grey8"));
    CHECK(matchesExpected("grey16"));
}

TEST_CASE(png_decodes_rgb_grey_alpha_and_rgba) {
    CHECK(matchesExpected("rgb8"));
    CHECK(matchesExpected("rgb16"));
    CHECK(matchesExpected("greyalpha8"));
    CHECK(matchesExpected("greyalpha16"));
    CHECK(matchesExpected("rgba8"));
    CHECK(matchesExpected("rgba16"));
}

TEST_CASE(png_decodes_every_palette_depth) {
    CHECK(matchesExpected("palette1"));
    CHECK(matchesExpected("palette2"));
    CHECK(matchesExpected("palette4"));
    CHECK(matchesExpected("palette8"));
}

TEST_CASE(png_applies_transparency_chunks) {
    CHECK(matchesExpected("grey4_trns"));
    CHECK(matchesExpected("rgb8_trns"));
    CHECK(matchesExpected("palette4_trns"));

    // The first two pixels of rgb8_trns share the transparent color
    Image decoded;
    std::string error;
    CHECK(decode(readBytes("rgb8_trns.png"), decoded, error));
    CHECK(decoded.pixels[3] == 0);
    CHECK(decoded.pixels[7] == 0);
}

TEST_CASE(png_decodes_stored_and_dynamic_blocks) {
    CHECK(matchesExpected("stored"));

    Image decoded;
    std::string error;
    CHECK(decode(readBytes("gradient.png"), decoded, error));
    CHECK(decoded.width == 300);
    CHECK(decoded.height == 200);
    bool same = decoded.pixels.size() == 300u * 200u * 4u;
    for (u32 y = 0; same && y < 200; ++y) {
        for (u32 x = 0; same && x < 300; ++x) {
            const u8* p = decoded.pixels.data() + (y * 300 + x) * 4;
            same = p[0] == (x * 255) / 300 && p[1] == (y * 255) / 200 && p[2] == ((x ^ y) & 0xFF) && p[3] == 255;
        }
    }
    CHECK(same);
}

TEST_CASE(png_refuses_interlaced_with_a_clear_message) {
    Image decoded;
    std::string error;
    CHECK(!decode(readBytes("interlaced.png"), decoded, error));
    CHECK(error.find("interlaced") != std::string::npos);
}

TEST_CASE(png_refuses_damaged_files) {
    const std::vector<u8> good = readBytes("rgba8.png");
    Image decoded;
    std::string error;
    CHECK(decode(good, decoded, error));

    // Not a PNG
    std::vector<u8> bytes = good;
    bytes[1] = 'X';
    CHECK(!isPng(bytes.data(), bytes.size()));
    CHECK(!decode(bytes, decoded, error));

    // A wrong checksum
    bytes = good;
    bytes[20] ^= 0x01;
    CHECK(!decode(bytes, decoded, error));
    CHECK(error.find("checksum") != std::string::npos);

    // Cut short anywhere
    bool allRefused = true;
    for (std::size_t size = 0; size < good.size(); size += 7) {
        allRefused = allRefused && !decodePng(good.data(), size, decoded, error);
    }
    CHECK(allRefused);

    // Header values PNG doesn't allow
    CHECK(!decode(withHeaderByte(good, 3, 0), decoded, error));          // width 0
    CHECK(!decode(withHeaderByte(good, 8, 3), decoded, error));          // RGBA at 3 bits
    CHECK(!decode(withHeaderByte(good, 9, 5), decoded, error));          // color type 5
    CHECK(!decode(withHeaderByte(good, 10, 1), decoded, error));         // compression method 1

    // Too large: width 40000
    std::vector<u8> wide = withHeaderByte(good, 1, 0);
    wide = withHeaderByte(wide, 2, 0x9C);
    wide = withHeaderByte(wide, 3, 0x40);
    CHECK(!decode(wide, decoded, error));
    CHECK(error.find("too large") != std::string::npos);

    // The decoder leaves nothing behind on failure
    CHECK(decoded.pixels.empty());
}

TEST_CASE(png_refuses_damaged_image_data) {
    // Flipping bits inside the compressed data must fail cleanly (bad code, distance, or checksum), never crash
    const std::vector<u8> good = readBytes("gradient.png");
    std::size_t idat = 0;
    for (std::size_t i = 8; i + 4 <= good.size(); ++i) {
        if (std::memcmp(good.data() + i, "IDAT", 4) == 0) { idat = i; break; }
    }
    CHECK(idat != 0);

    bool allRefused = true;
    for (std::size_t k = 6; k < 90; k += 3) {
        std::vector<u8> bytes = good;
        const std::size_t length = (u32(bytes[idat - 4]) << 24) | (u32(bytes[idat - 3]) << 16) | (u32(bytes[idat - 2]) << 8) | bytes[idat - 1];
        bytes[idat + 4 + k] ^= 0x5A;
        putU32(bytes.data() + idat + 4 + length, crc(bytes.data() + idat, length + 4));
        Image decoded;
        std::string error;
        allRefused = allRefused && !decode(bytes, decoded, error);
    }
    CHECK(allRefused);
}

TEST_CASE(inflate_reads_a_hand_made_stored_block) {
    // zlib header, one final stored block holding "abc", then the Adler-32 of "abc"
    const u8 stream[] = { 0x78, 0x01, 0x01, 0x03, 0x00, 0xFC, 0xFF, 'a', 'b', 'c', 0x02, 0x4D, 0x01, 0x27 };
    std::vector<u8> out;
    std::string error;
    CHECK(inflateZlib(stream, sizeof(stream), out, error, 3));
    CHECK(out.size() == 3 && out[0] == 'a' && out[2] == 'c');

    // Larger than the caller allows
    CHECK(!inflateZlib(stream, sizeof(stream), out, error, 2));
}

TEST_CASE(deflate_round_trips_through_inflate) {
    // Empty, tiny, one long run, text that repeats, noise, and smooth data longer than the window and one block
    std::vector<std::vector<u8>> inputs = { {}, { 7 }, { 1, 2 }, { 'a', 'b', 'c', 'a', 'b', 'c', 'a', 'b', 'c', 'a' } };
    inputs.push_back(std::vector<u8>(100000, 0));
    std::vector<u8> text;
    for (int i = 0; i < 4000; ++i) {
        const std::string line = "face " + std::to_string(i % 37) + " uses material " + std::to_string(i % 5) + "\n";
        text.insert(text.end(), line.begin(), line.end());
    }
    inputs.push_back(text);
    std::vector<u8> noise(200000);
    u32 seed = 12345;
    for (u8& byte : noise) {
        seed = seed * 1664525u + 1013904223u;
        byte = static_cast<u8>(seed >> 24);
    }
    inputs.push_back(noise);
    std::vector<u8> smooth(600000);
    for (std::size_t i = 0; i < smooth.size(); ++i) smooth[i] = static_cast<u8>((i / 700) * 3 + (i % 5 == 0 ? noise[i % noise.size()] & 3 : 0));
    inputs.push_back(smooth);

    bool allSame = true;
    for (const std::vector<u8>& input : inputs) {
        const std::vector<u8> packed = deflateZlib(input.data(), input.size());
        std::vector<u8> back;
        std::string error;
        allSame = allSame && inflateZlib(packed.data(), packed.size(), back, error, input.size()) && back == input;
    }
    CHECK(allSame);

    // Runs and repeats shrink a lot; noise falls back to stored blocks, a few bytes over its own size
    CHECK(deflateZlib(inputs[4].data(), inputs[4].size()).size() < 300);
    CHECK(deflateZlib(text.data(), text.size()).size() < text.size() / 8);
    CHECK(deflateZlib(noise.data(), noise.size()).size() < noise.size() + 64);
}

TEST_CASE(png_encoder_compresses) {
    // A flat color, as a new texture is, and a soft gradient with a hard-edged shape, as paint is
    image::Image flat;
    flat.width = flat.height = 512;
    flat.pixels.resize(std::size_t(512) * 512 * 4);
    for (std::size_t i = 0; i < flat.pixels.size(); i += 4) {
        flat.pixels[i] = 200;
        flat.pixels[i + 1] = 120;
        flat.pixels[i + 2] = 40;
        flat.pixels[i + 3] = 255;
    }
    const std::vector<image::u8> flatPng = image::encodePng(flat);
    CHECK(flatPng.size() < 4096);

    image::Image painted = flat;
    for (u32 y = 0; y < 512; ++y) {
        for (u32 x = 0; x < 512; ++x) {
            u8* p = painted.pixels.data() + (std::size_t(y) * 512 + x) * 4;
            p[0] = static_cast<u8>(x / 2);
            p[1] = static_cast<u8>(y / 2);
            p[2] = static_cast<u8>((x + y) / 4);
            if ((x - 256) * (x - 256) + (y - 256) * (y - 256) < 100 * 100) p[3] = 0;
        }
    }
    const std::vector<image::u8> paintedPng = image::encodePng(painted);
    CHECK(paintedPng.size() < painted.pixels.size() / 20);

    image::Image back;
    std::string error;
    CHECK(image::decodePng(flatPng.data(), flatPng.size(), back, error));
    CHECK(back.pixels == flat.pixels);
    CHECK(image::decodePng(paintedPng.data(), paintedPng.size(), back, error));
    CHECK(back.pixels == painted.pixels);
}

TEST_CASE(png_encodes_and_reads_back) {
    // Every byte value over many rows, and a 1 x 1
    image::Image picture;
    picture.width = 300;
    picture.height = 70;
    picture.pixels.resize(std::size_t(picture.width) * picture.height * 4);
    for (std::size_t i = 0; i < picture.pixels.size(); ++i) picture.pixels[i] = static_cast<image::u8>(i * 7 + i / 300);

    const std::vector<image::u8> png = image::encodePng(picture);
    CHECK(image::isPng(png.data(), png.size()));
    image::Image back;
    std::string error;
    CHECK(image::decodePng(png.data(), png.size(), back, error));
    CHECK(back.width == 300 && back.height == 70);
    CHECK(back.pixels == picture.pixels);

    image::Image dot;
    dot.width = dot.height = 1;
    dot.pixels = { 10, 20, 30, 40 };
    const std::vector<image::u8> small = image::encodePng(dot);
    CHECK(image::decodePng(small.data(), small.size(), back, error));
    CHECK(back.pixels == dot.pixels);
}
