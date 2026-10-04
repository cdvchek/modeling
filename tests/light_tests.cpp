#include "test.hpp"
#include "scene/lights/light_collection.hpp"

TEST_CASE(light_add_get_edit) {
    LightCollection lights;
    Light light;
    light.name = "key";
    light.intensity = 2.0f;

    LightHandle handle = lights.add(light);
    CHECK(lights.isValid(handle));
    CHECK(lights.count() == 1);
    CHECK(lights.get(handle).name == "key");

    lights.get(handle).intensity = 5.0f;
    CHECK(lights.get(handle).intensity == 5.0f);
}

TEST_CASE(light_replace) {
    LightCollection lights;
    LightHandle handle = lights.add(Light{});

    Light sun;
    sun.type = LightType::Directional;
    CHECK(lights.replace(handle, sun));
    CHECK(lights.get(handle).type == LightType::Directional);
    CHECK(!lights.replace(INVALID_LIGHT, sun));
}

TEST_CASE(light_remove_invalidates_handle) {
    LightCollection lights;
    LightHandle first = lights.add(Light{});
    lights.remove(first);

    CHECK(!lights.isValid(first));
    CHECK(lights.tryGet(first) == nullptr);
    CHECK(lights.count() == 0);

    LightHandle second = lights.add(Light{});
    CHECK(second.index == first.index);
    CHECK(second != first);
    CHECK(lights.handles().size() == 1);
}

TEST_CASE(light_ambient_clamps) {
    LightCollection lights;
    lights.setAmbientStrength(1.5f);
    CHECK(lights.getAmbient().strength == 1.0f);

    lights.setAmbientColor(Vec3(-1.0f, 0.5f, 2.0f));
    CHECK(lights.getAmbient().color.x == 0.0f);
    CHECK(lights.getAmbient().color.y == 0.5f);
    CHECK(lights.getAmbient().color.z == 1.0f);
}
