#pragma once

#include "renderer/renderer.hpp"
#include "scene/scene.hpp"

class DebugRenderer {
public:
    void render(
        IRenderer& renderer,
        const Scene& scene
    );

private:
    void drawHalfEdges(
        IRenderer& renderer,
        const Object& object
    );

    void drawHalfEdge(
        IRenderer& renderer,
        const MeshData& mesh,
        EdgeHandle edge
    );

    void drawHoveredElement(
        IRenderer& renderer,
        const MeshData& mesh
    );

    void drawArrow(
        IRenderer& renderer,
        const Vec3& start,
        const Vec3& end,
        const Vec3& normal
    );
};