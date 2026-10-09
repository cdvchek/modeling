#pragma once

#include "scene/camera.hpp"
#include "scene/objects/object_collection.hpp"
#include "scene/lights/light_collection.hpp"
#include "scene/references/reference_collection.hpp"
#include "scene/materials/material_collection.hpp"
#include "scene/textures/texture_collection.hpp"
#include "scene/selection/selection.hpp"

struct Scene {
    Camera camera;
    ObjectCollection objects;
    LightCollection lights;
    ReferenceCollection references;
    MaterialCollection materials;
    TextureCollection textures;
    Selection selection;

    // The object being edited; null when there are no objects
    Object* activeObject() { return objects.tryGet(selection.getActiveObject()); }
    const Object* activeObject() const { return objects.tryGet(selection.getActiveObject()); }
};
