#include "application/application.hpp"
#include "core/assets/meshes/mesh_factory.hpp"

void Application::initializeCamera(AppContext& ctx) {
    ctx.camera.position = Vec3(0.0f, 0.0f, 3.0f);
    ctx.camera.target = Vec3();
    ctx.camera.up = Vec3(0.0f, 1.0f, 0.0f);
}

void Application::loadTestScene(AppContext& ctx) {
    ctx.testObject.meshData = MeshFactory::createCube();
    ctx.testObject.gpuMesh.create(ctx.testObject.meshData);
}