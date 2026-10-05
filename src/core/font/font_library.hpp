#pragma once

#include <array>
#include "core/font/bitmap_font.hpp"

enum class FontId : u32 {
    Console,
    UI,
    Count
};

class FontLibrary {
public:
    bool loadEmbedded();
    const BitmapFont& get(FontId id) const;

private:
    std::array<BitmapFont, static_cast<u32>(FontId::Count)> m_fonts;
};
