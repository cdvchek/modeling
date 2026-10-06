#include "application/application.hpp"
#include "scene/mesh/mesh_types.hpp"

void Application::initializeCamera(AppContext& ctx) {
    ctx.scene.camera.position = Vec3(0.0f, 0.0f, 3.0f);
    ctx.scene.camera.target = Vec3();
    ctx.scene.camera.up = Vec3(0.0f, 1.0f, 0.0f);
}

void Application::loadTestScene(AppContext& ctx) {
    const ObjectHandle cube = ctx.scene.objects.add("Cube", PresetMesh::Cube);
    ctx.scene.selection.setActiveObject(cube);

    Light sun;
    sun.name = "Sun";
    sun.type = LightType::Directional;
    sun.direction = Vec3(0.4f, -1.0f, -0.6f).normalized();
    sun.intensity = 0.6f;
    ctx.scene.lights.add(sun);
}