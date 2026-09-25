#include "core/font/bitmap_font.hpp"

#include <fstream>


static bool hexToAlpha(char c, u8& alpha) {

    if (c >= '0' && c <= '9') {

        alpha = static_cast<u8>(
            (c - '0') * 17
        );

        return true;
    }

    if (c >= 'A' && c <= 'F') {

        alpha = static_cast<u8>(
            (10 + c - 'A') * 17
        );

        return true;
    }

    return false;
}


bool BitmapFont::load(const std::string& path) {

    std::ifstream file(path);

    if (!file.is_open()) {
        return false;
    }


    // --------------------------------
    // Header
    // --------------------------------

    std::string magic;

    file >> magic;

    if (magic != "BMF1") {
        return false;
    }


    std::string label;

    u32 width;
    u32 height;

    file >> label >> width;

    if (label != "width" ||
        width != BitmapGlyph::WIDTH) {

        return false;
    }

    file >> label >> height;

    if (label != "height" ||
        height != BitmapGlyph::HEIGHT) {

        return false;
    }


    // --------------------------------
    // Glyphs
    // --------------------------------

    u32 character;

    while (file >> character) {

        if (character < FIRST_CHAR ||
            character > LAST_CHAR) {

            return false;
        }

        u32 index = character - FIRST_CHAR;

        BitmapGlyph& glyph = m_glyphs[index];

        std::string row;

        for (u32 y = 0;
             y < BitmapGlyph::HEIGHT;
             ++y) {

            if (!(file >> row)) {
                return false;
            }

            if (row.size() != BitmapGlyph::WIDTH) {
                return false;
            }


            for (u32 x = 0;
                 x < BitmapGlyph::WIDTH;
                 ++x) {

                u8 alpha;

                if (!hexToAlpha(row[x], alpha)) {
                    return false;
                }

                glyph.pixels[
                    y * BitmapGlyph::WIDTH + x
                ] = alpha;
            }
        }

        m_loaded[index] = true;
    }

    return true;
}


const BitmapGlyph* BitmapFont::getGlyph(
    char character
) const {

    u32 code =
        static_cast<unsigned char>(character);

    if (code < FIRST_CHAR ||
        code > LAST_CHAR) {

        return nullptr;
    }

    u32 index = code - FIRST_CHAR;

    if (!m_loaded[index]) {
        return nullptr;
    }

    return &m_glyphs[index];
}