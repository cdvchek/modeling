#include "test.hpp"

#include "core/font/bitmap_font.hpp"
#include "core/font/embedded_fonts.hpp"
#include "core/font/font_library.hpp"

TEST_CASE(embedded_console_font_loads_every_glyph) {
    BitmapFont font;
    CHECK(font.loadFromMemory(EmbeddedFonts::console, EmbeddedFonts::consoleSize));

    for (u32 character = BitmapFont::FIRST_CHAR; character <= BitmapFont::LAST_CHAR; ++character) {
        CHECK(font.getGlyph(static_cast<char>(character)) != nullptr);
    }
}

TEST_CASE(embedded_fonts_load_with_their_sizes) {
    FontLibrary fonts;
    CHECK(fonts.loadEmbedded());

    CHECK(fonts.get(FontId::Console).getGlyphWidth() == 16);
    CHECK(fonts.get(FontId::Console).getGlyphHeight() == 24);
    CHECK(fonts.get(FontId::UI).getGlyphWidth() == 10);
    CHECK(fonts.get(FontId::UI).getGlyphHeight() == 16);

    for (u32 character = BitmapFont::FIRST_CHAR; character <= BitmapFont::LAST_CHAR; ++character) {
        CHECK(fonts.get(FontId::UI).getGlyph(static_cast<char>(character)) != nullptr);
    }
}
