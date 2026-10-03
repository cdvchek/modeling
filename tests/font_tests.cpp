#include "test.hpp"

#include "core/font/bitmap_font.hpp"
#include "core/font/embedded_fonts.hpp"

TEST_CASE(embedded_console_font_loads_every_glyph) {
    BitmapFont font;
    CHECK(font.loadFromMemory(EmbeddedFonts::console, EmbeddedFonts::consoleSize));

    for (u32 character = BitmapFont::FIRST_CHAR; character <= BitmapFont::LAST_CHAR; ++character) {
        CHECK(font.getGlyph(static_cast<char>(character)) != nullptr);
    }
}
