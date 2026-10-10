#include "scene/materials/material_collection.hpp"

namespace {
    bool sameVec3(const Vec3& a, const Vec3& b) {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }
}

bool Material::sameLook(const Material& other) const {
    return sameVec3(baseColor, other.baseColor) && roughness == other.roughness && metallic == other.metallic
        && sameVec3(emissiveColor, other.emissiveColor) && emissiveStrength == other.emissiveStrength
        && opacity == other.opacity && alphaMode == other.alphaMode && alphaCutoff == other.alphaCutoff
        && doubleSided == other.doubleSided && baseColorMap == other.baseColorMap;
}

const char* alphaModeName(AlphaMode mode) {
    switch (mode) {
        case AlphaMode::Cutout: return "cutout";
        case AlphaMode::Blend: return "blend";
        default: return "opaque";
    }
}

MaterialCollection::MaterialCollection() {
    Material defaults;
    defaults.name = "Default";
    m_default = m_materials.insert(defaults);
}

MaterialHandle MaterialCollection::add(const Material& material) {
    return m_materials.insert(material);
}

bool MaterialCollection::remove(MaterialHandle handle) {
    if (isDefault(handle) || !isValid(handle)) return false;
    m_materials.remove(handle);
    return true;
}

bool MaterialCollection::isValid(MaterialHandle handle) const {
    return m_materials.isValid(handle);
}

bool MaterialCollection::isDefault(MaterialHandle handle) const {
    return handle == m_default;
}

MaterialHandle MaterialCollection::resolve(MaterialHandle handle) const {
    return isValid(handle) ? handle : m_default;
}

Material& MaterialCollection::get(MaterialHandle handle) {
    return m_materials.get(handle);
}

const Material& MaterialCollection::get(MaterialHandle handle) const {
    return m_materials.get(handle);
}

Material* MaterialCollection::tryGet(MaterialHandle handle) {
    return m_materials.tryGet(handle);
}

const Material* MaterialCollection::tryGet(MaterialHandle handle) const {
    return m_materials.tryGet(handle);
}

std::vector<MaterialHandle> MaterialCollection::handles() const {
    // Default lives in the first slot, so slot order already puts it first
    return m_materials.getActiveHandles();
}

MaterialHandle MaterialCollection::handleAt(u32 slot) const {
    if (slot >= m_materials.size()) return INVALID_MATERIAL;
    return m_materials.getHandle(slot);
}

u32 MaterialCollection::count() const {
    return m_materials.activeSize();
}

std::string MaterialCollection::uniqueName(const std::string& base) const {
    const auto taken = [this](const std::string& name) {
        for (MaterialHandle handle : handles()) {
            if (m_materials.get(handle).name == name) return true;
        }
        return false;
    };

    if (!taken(base)) return base;

    for (u32 number = 2;; ++number) {
        const std::string name = base + " " + std::to_string(number);
        if (!taken(name)) return name;
    }
}

MaterialHandle MaterialCollection::adopt(const Material& material, bool& added) {
    const MaterialHandle identical = findIdentical(material);
    added = identical.isNull();
    if (!added) return identical;

    Material renamed = material;
    renamed.name = uniqueName(material.name);
    return add(renamed);
}

MaterialHandle MaterialCollection::findIdentical(const Material& material) const {
    for (MaterialHandle handle : handles()) {
        const Material& existing = m_materials.get(handle);
        if (existing.name == material.name && existing.sameLook(material)) return handle;
    }
    return INVALID_MATERIAL;
}
