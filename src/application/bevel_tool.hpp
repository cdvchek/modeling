#pragma once

#include <types>

#include "core/math/vec3.hpp"
#include "scene/mesh/mesh_data.hpp"
#include "scene/selection/selection.hpp"

struct BevelTool {
    u32 objectIndex = 0;
    BevelSession session;

    Vec3 pivot;
    f32 startDistance = 0.0f;

    Selection savedSelection;
};
