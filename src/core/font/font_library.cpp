#include "core/font/font_library.hpp"
#include "core/font/embedded_fonts.hpp"

bool FontLibrary::loadEmbedded() {
    return m_fonts[static_cast<u32>(FontId::Console)].loadFromMemory(EmbeddedFonts::console, EmbeddedFonts::consoleSize)
        && m_fonts[static_cast<u32>(FontId::UI)].loadFromMemory(EmbeddedFonts::ui, EmbeddedFonts::uiSize);
}

const BitmapFont& FontLibrary::get(FontId id) const {
    return m_fonts[static_cast<u32>(id)];
}
