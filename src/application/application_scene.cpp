#include "application/application.hpp"
#include "scene/mesh_factory.hpp"

void Application::initializeCamera(AppContext& ctx) {
    ctx.scene.camera.position = Vec3(0.0f, 0.0f, 3.0f);
    ctx.scene.camera.target = Vec3();
    ctx.scene.camera.up = Vec3(0.0f, 1.0f, 0.0f);
}

void Application::loadTestScene(AppContext& ctx) {
    ctx.scene.objects.get(0).meshData = MeshFactory::createCube();
    ctx.scene.objects.get(0).gpuMesh.create(ctx.scene.objects.get(0).meshData);
}