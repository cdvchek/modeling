#pragma once

#include <string>
#include <vector>
#include "scene/materials/material.hpp"

// The project's materials, shared by the objects that use them. There's always a Default material, which can be
// edited but not removed. An object's material handle that isn't valid (none set, or its material was removed)
// means Default, so removing a material puts its objects back on Default without touching them.
class MaterialCollection {
public:
    MaterialCollection();

    MaterialHandle add(const Material& material);
    // False for Default
    bool remove(MaterialHandle handle);

    bool isValid(MaterialHandle handle) const;
    bool isDefault(MaterialHandle handle) const;

    MaterialHandle defaultMaterial() const { return m_default; }
    // handle when it's valid, otherwise Default
    MaterialHandle resolve(MaterialHandle handle) const;

    Material& get(MaterialHandle handle);
    const Material& get(MaterialHandle handle) const;
    Material* tryGet(MaterialHandle handle);
    const Material* tryGet(MaterialHandle handle) const;

    // Default first, then the rest in slot order
    std::vector<MaterialHandle> handles() const;
    MaterialHandle handleAt(u32 slot) const;
    u32 count() const;

    // base, or "base N" with the lowest N that isn't taken
    std::string uniqueName(const std::string& base) const;
    // Changes whenever a material is added or removed (copies keep it), so face groups built against it can be checked
    u64 stamp() const { return m_materials.stamp(); }

    // A material with the same name and every value the same, if there is one (import reuses it)
    MaterialHandle findIdentical(const Material& material) const;
    // A material brought in from elsewhere (import): the identical one if there is one, otherwise it's added,
    // renamed if its name is taken. added says which.
    MaterialHandle adopt(const Material& material, bool& added);

private:
    DynamicArray<Material> m_materials;
    MaterialHandle m_default;
};
