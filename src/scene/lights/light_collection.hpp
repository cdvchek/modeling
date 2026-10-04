#pragma once

#include <vector>
#include "scene/lights/light.hpp"

class LightCollection {
public:
    LightHandle add(const Light& light);
    void remove(LightHandle handle);
    bool replace(LightHandle handle, const Light& light);

    bool isValid(LightHandle handle) const;

    Light& get(LightHandle handle);
    const Light& get(LightHandle handle) const;

    Light* tryGet(LightHandle handle);
    const Light* tryGet(LightHandle handle) const;

    std::vector<LightHandle> handles() const;
    u32 count() const;

    const AmbientLight& getAmbient() const;
    void setAmbientColor(const Vec3& color);
    void setAmbientStrength(f32 strength);

private:
    DynamicArray<Light> m_lights;
    AmbientLight m_ambient;
};
