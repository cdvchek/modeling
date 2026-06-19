#pragma once

#include "scene/camera.hpp"
#include "scene/object.hpp"
#include <vector>

struct Scene {
    Camera camera;
    std::vector<Object> objects;
};