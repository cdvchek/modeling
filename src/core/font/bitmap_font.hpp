#pragma once

#include <array>
#include <string>
#include <types>

struct BitmapGlyph {

    static constexpr u32 WIDTH = 16;
    static constexpr u32 HEIGHT = 24;

    std::array<u8, WIDTH * HEIGHT> pixels{};
};

class BitmapFont {
public:
    static constexpr u32 FIRST_CHAR = 32;
    static constexpr u32 LAST_CHAR = 126;
    static constexpr u32 GLYPH_COUNT = LAST_CHAR - FIRST_CHAR + 1;
    
    bool load(const std::string& path);
    const BitmapGlyph* getGlyph(char character) const;

private:
    std::array<BitmapGlyph, GLYPH_COUNT> m_glyphs{};
    std::array<bool, GLYPH_COUNT> m_loaded{};
};