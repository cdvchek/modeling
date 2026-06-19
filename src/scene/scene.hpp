#pragma once

#include "scene/camera.hpp"
#include "scene/object_collection.hpp"
#include <vector>

struct Scene {
    Camera camera;
    ObjectCollection objects;
};