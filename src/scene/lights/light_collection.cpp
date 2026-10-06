#include "scene/lights/light_collection.hpp"

#include <algorithm>

LightHandle LightCollection::add(const Light& light) {
    return m_lights.insert(light);
}

void LightCollection::remove(LightHandle handle) {
    m_lights.remove(handle);
}

bool LightCollection::replace(LightHandle handle, const Light& light) {
    Light* existing = m_lights.tryGet(handle);
    if (!existing) return false;

    *existing = light;
    return true;
}

bool LightCollection::isValid(LightHandle handle) const {
    return m_lights.isValid(handle);
}

Light& LightCollection::get(LightHandle handle) {
    return m_lights.get(handle);
}

const Light& LightCollection::get(LightHandle handle) const {
    return m_lights.get(handle);
}

Light* LightCollection::tryGet(LightHandle handle) {
    return m_lights.tryGet(handle);
}

const Light* LightCollection::tryGet(LightHandle handle) const {
    return m_lights.tryGet(handle);
}

std::vector<LightHandle> LightCollection::handles() const {
    return m_lights.getActiveHandles();
}

LightHandle LightCollection::handleAt(u32 slot) const {
    if (slot >= m_lights.size()) return INVALID_LIGHT;
    return m_lights.getHandle(slot);
}

std::string LightCollection::nextName() const {
    constexpr std::string_view PREFIX = "Light ";

    u32 highest = 0;
    for (LightHandle handle : handles()) {
        const std::string& name = m_lights.get(handle).name;
        if (name.size() <= PREFIX.size() || name.compare(0, PREFIX.size(), PREFIX) != 0) continue;

        const std::string digits = name.substr(PREFIX.size());
        if (digits.find_first_not_of("0123456789") != std::string::npos) continue;

        highest = std::max(highest, static_cast<u32>(std::stoul(digits)));
    }

    return std::string(PREFIX) + std::to_string(highest + 1);
}

u32 LightCollection::count() const {
    return m_lights.activeSize();
}

const AmbientLight& LightCollection::getAmbient() const {
    return m_ambient;
}

void LightCollection::setAmbientColor(const Vec3& color) {
    m_ambient.color = Vec3(
        std::clamp(color.x, 0.0f, 1.0f),
        std::clamp(color.y, 0.0f, 1.0f),
        std::clamp(color.z, 0.0f, 1.0f)
    );
}

void LightCollection::setAmbientStrength(f32 strength) {
    m_ambient.strength = std::clamp(strength, 0.0f, 1.0f);
}
