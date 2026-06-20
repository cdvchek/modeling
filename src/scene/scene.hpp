#pragma once

#include "scene/camera.hpp"
#include "scene/object_collection.hpp"
#include "scene/selection/selection.hpp"

struct Scene {
    Camera camera;
    ObjectCollection objects;
    Selection selection;
};