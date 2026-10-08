#include "scene/references/reference_collection.hpp"

f32 ReferenceImage::aspect() const {
    if (!picture || picture->height == 0) return 1.0f;
    return static_cast<f32>(picture->width) / static_cast<f32>(picture->height);
}

Transform ReferenceImage::transform() const {
    Transform result;
    result.position = position;
    result.rotation = rotation;
    result.scale = Vec3(size * aspect(), size, 1.0f);
    return result;
}

const char* referenceDepthName(ReferenceDepth depth) {
    switch (depth) {
        case ReferenceDepth::Behind: return "behind";
        case ReferenceDepth::InFront: return "front";
        default: return "scene";
    }
}

ReferenceHandle ReferenceCollection::add(ReferenceImage image) {
    return m_images.insert(std::move(image));
}

void ReferenceCollection::remove(ReferenceHandle handle) {
    m_images.remove(handle);
}

bool ReferenceCollection::isValid(ReferenceHandle handle) const {
    return m_images.isValid(handle);
}

ReferenceImage& ReferenceCollection::get(ReferenceHandle handle) {
    return m_images.get(handle);
}

const ReferenceImage& ReferenceCollection::get(ReferenceHandle handle) const {
    return m_images.get(handle);
}

ReferenceImage* ReferenceCollection::tryGet(ReferenceHandle handle) {
    return m_images.tryGet(handle);
}

const ReferenceImage* ReferenceCollection::tryGet(ReferenceHandle handle) const {
    return m_images.tryGet(handle);
}

std::vector<ReferenceHandle> ReferenceCollection::handles() const {
    return m_images.getActiveHandles();
}

ReferenceHandle ReferenceCollection::handleAt(u32 slot) const {
    if (slot >= m_images.size()) return INVALID_REFERENCE;
    return m_images.getHandle(slot);
}

u32 ReferenceCollection::count() const {
    return m_images.activeSize();
}

std::string ReferenceCollection::uniqueName(const std::string& base) const {
    const auto taken = [this](const std::string& name) {
        for (ReferenceHandle handle : handles()) {
            if (m_images.get(handle).name == name) return true;
        }
        return false;
    };

    if (!taken(base)) return base;

    for (u32 number = 2;; ++number) {
        const std::string name = base + " " + std::to_string(number);
        if (!taken(name)) return name;
    }
}
