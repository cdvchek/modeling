#pragma once

#include <string>
#include <vector>
#include "scene/references/reference_image.hpp"

class ReferenceCollection {
public:
    ReferenceHandle add(ReferenceImage image);
    void remove(ReferenceHandle handle);

    bool isValid(ReferenceHandle handle) const;

    ReferenceImage& get(ReferenceHandle handle);
    const ReferenceImage& get(ReferenceHandle handle) const;

    ReferenceImage* tryGet(ReferenceHandle handle);
    const ReferenceImage* tryGet(ReferenceHandle handle) const;

    std::vector<ReferenceHandle> handles() const;
    ReferenceHandle handleAt(u32 slot) const;
    u32 count() const;

    // base, or "base N" with the lowest N that isn't taken
    std::string uniqueName(const std::string& base) const;

private:
    DynamicArray<ReferenceImage> m_images;
};
