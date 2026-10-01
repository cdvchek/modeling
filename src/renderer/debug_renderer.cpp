#include "debug_renderer.hpp"

#include <cmath>

void DebugRenderer::render(
    IRenderer& renderer,
    const Scene& scene
) {
    // Adjust this loop to however your ObjectCollection is currently exposed.
    for (const Object& object : scene.objects.all()) {
        drawHalfEdges(renderer, object);
    }
}


void DebugRenderer::drawHalfEdges(
    IRenderer& renderer,
    const Object& object
) {
    const MeshData& mesh = object.meshData;

    // Use your existing active-handle getter here.
    const std::vector<EdgeHandle> edges = mesh.getEdgeHandles();

    for (const EdgeHandle edge : edges) {
        drawHalfEdge(renderer, mesh, edge);
    }
}


void DebugRenderer::drawHalfEdge(
    IRenderer& renderer,
    const MeshData& mesh,
    EdgeHandle edgeHandle
) {
    const Edge* edge = mesh.getEdge(edgeHandle);

    // A half-edge without a face doesn't have a face to inset toward.
    if (!mesh.isValidHandle(edge->face)) return;
    if (!mesh.isValidHandle(edge->prev)) return;
    if (!mesh.isValidHandle(edge->tip)) return;

    const Edge* prev = mesh.getEdge(edge->prev);

    if (!mesh.isValidHandle(prev->tip)) return;

    const Vertex* prevTip = mesh.getVertex(prev->tip);
    const Vertex* edgeTip = mesh.getVertex(edge->tip);
    if (!prevTip || !edgeTip) return;
    const Vec3& startPosition = prevTip->position;
    const Vec3& endPosition = edgeTip->position;


    // ---------------------------------------------------------
    // Find the center of the face.
    // ---------------------------------------------------------

    Vec3 faceCenter{0.0f, 0.0f, 0.0f};
    u32 vertexCount = 0;

    const Face* face = mesh.getFace(edge->face);

    if (!face) return;

    EdgeHandle current = face->edge;

    do {
        const Edge* currentEdge = mesh.getEdge(current);
        if (!currentEdge) return;

        const Vertex* currentTip = mesh.getVertex(currentEdge->tip);
        if (!currentTip) return;

        faceCenter += currentTip->position;

        ++vertexCount;

        current = currentEdge->next;

    } while (current != face->edge);

    if (vertexCount == 0) return;

    faceCenter /= static_cast<f32>(vertexCount);


    // ---------------------------------------------------------
    // Move the half-edge slightly toward the center of its face.
    //
    // This separates paired half-edges visually.
    // ---------------------------------------------------------

    constexpr f32 FACE_OFFSET = 0.08f;

    Vec3 start = startPosition + (faceCenter - startPosition) * FACE_OFFSET;

    Vec3 end = endPosition + (faceCenter - endPosition) * FACE_OFFSET;

    // ---------------------------------------------------------
    // Shorten it slightly so arrows don't touch vertices.
    // ---------------------------------------------------------

    constexpr f32 END_OFFSET = 0.08f;

    Vec3 direction = end - start;

    start += direction * END_OFFSET;
    end   -= direction * END_OFFSET;

    Vec3 normal = mesh.getFaceNormal(edge->face);

    drawArrow(renderer, start, end, normal);
}


void DebugRenderer::drawArrow(IRenderer& renderer, const Vec3& start, const Vec3& end, const Vec3& normal) {
    Vec3 direction = end - start;
    f32 length = direction.length();

    if (length <= 0.0001f) return;

    direction /= length;

    Vec3 side = Vec3::cross(normal, direction);

    if (side.length() <= 0.0001f) return;

    side = side.normalized();

    renderer.drawDebugLine(start, end);

    constexpr f32 ARROW_LENGTH_FACTOR = 0.08f;
    constexpr f32 ARROW_WIDTH_FACTOR  = 0.04f;

    f32 arrowLength = length * ARROW_LENGTH_FACTOR;
    f32 arrowWidth  = length * ARROW_WIDTH_FACTOR;

    Vec3 arrowBase = end - direction * arrowLength;

    Vec3 left = arrowBase + side * arrowWidth;

    Vec3 right = arrowBase - side * arrowWidth;

    renderer.drawDebugLine(end, left);
    renderer.drawDebugLine(end, right);
}


void DebugRenderer::drawHoveredElement(
    IRenderer& renderer,
    const MeshData& mesh
) {
    // We'll implement this after basic half-edge
    // visualization is working.
}