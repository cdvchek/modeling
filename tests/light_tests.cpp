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

TEST_CASE(light_handle_at_slot) {
    LightCollection lights;
    LightHandle first = lights.add(Light{});
    LightHandle second = lights.add(Light{});

    CHECK(lights.handleAt(first.index) == first);
    CHECK(lights.handleAt(second.index) == second);
    CHECK(!lights.isValid(lights.handleAt(99)));

    lights.remove(first);
    CHECK(!lights.isValid(lights.handleAt(first.index)));
}

TEST_CASE(light_next_name_counts_past_the_highest) {
    LightCollection lights;
    CHECK(lights.nextName() == "Light 1");

    Light light;
    light.name = lights.nextName();
    const LightHandle first = lights.add(light);
    light.name = lights.nextName();
    lights.add(light);
    CHECK(lights.nextName() == "Light 3");

    // Deleting a lower number doesn't bring it back, and other names are ignored
    lights.remove(first);
    light.name = "Sun";
    lights.add(light);
    light.name = "Light 2b";
    lights.add(light);
    CHECK(lights.nextName() == "Light 3");
}
