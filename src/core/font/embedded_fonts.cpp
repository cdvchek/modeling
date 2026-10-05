#include "core/font/embedded_fonts.hpp"

// #embed paths are relative to assets/ (see --embed-dir in CMakeLists.txt).

namespace EmbeddedFonts {
    const unsigned char console[] = {
        #embed <fonts/console.bmf>
    };

    const std::size_t consoleSize = sizeof(console);

    const unsigned char ui[] = {
        #embed <fonts/ui.bmf>
    };

    const std::size_t uiSize = sizeof(ui);
}
