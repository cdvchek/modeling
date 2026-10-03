#include "core/font/embedded_fonts.hpp"

// #embed paths are relative to assets/ (--embed-dir in CMakeLists.txt).
// Editing the font file triggers a rebuild of this file.

namespace EmbeddedFonts {
    const unsigned char console[] = {
        #embed <fonts/console.bmf>
    };

    const std::size_t consoleSize = sizeof(console);
}
