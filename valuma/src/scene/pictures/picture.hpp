#pragma once

#include <string>
#include <vector>
#include <types>

// A PNG file as it was added, kept in the project. It never changes, so copies (undo) share it; reference images
// and textures both hold one.
struct Picture {
    std::string fileName;       // the name it had on disk, shown in the panel
    std::vector<u8> png;
    u32 width = 0;              // pixels
    u32 height = 0;
};
