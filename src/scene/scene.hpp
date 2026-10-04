#pragma once

#include "scene/camera.hpp"
#include "scene/objects/object_collection.hpp"
#include "scene/lights/light_collection.hpp"
#include "scene/selection/selection.hpp"

struct Scene {
    Camera camera;
    ObjectCollection objects;
    LightCollection lights;
    Selection selection;
};