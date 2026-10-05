#pragma once

#include <cstddef>
#include <istream>
#include <string>
#include <vector>
#include <types>

struct BitmapGlyph {
    std::vector<u8> pixels;    // glyphWidth * glyphHeight alpha values, row-major
};

class BitmapFont {
public:
    static constexpr u32 FIRST_CHAR = 32;
    static constexpr u32 LAST_CHAR = 126;
    static constexpr u32 GLYPH_COUNT = LAST_CHAR - FIRST_CHAR + 1;

    bool load(const std::string& path);
    bool loadFromMemory(const unsigned char* data, std::size_t size);
    const BitmapGlyph* getGlyph(char character) const;

    u32 getGlyphWidth() const { return m_glyphWidth; }
    u32 getGlyphHeight() const { return m_glyphHeight; }

private:
    bool parse(std::istream& file);

    u32 m_glyphWidth = 0;
    u32 m_glyphHeight = 0;
    std::vector<BitmapGlyph> m_glyphs = std::vector<BitmapGlyph>(GLYPH_COUNT);
    std::vector<bool> m_loaded = std::vector<bool>(GLYPH_COUNT, false);
};
