#include "debug_renderer.hpp"

#include <cmath>

void DebugRenderer::render(
    IRenderer& renderer,
    const Scene& scene,
    const Mat4& vp
) {
    for (const Object& object : scene.objects.all()) {
        drawHalfEdges(renderer, object, vp);
    }
}


void DebugRenderer::drawHalfEdges(
    IRenderer& renderer,
    const Object& object,
    const Mat4& vp
) {
    const MeshData& mesh = object.meshData;

    const std::vector<EdgeHandle> edges = mesh.getEdgeHandles();

    const Mat4 mvp = vp * object.transform.getMatrix();

    for (const EdgeHandle edge : edges) {
        drawHalfEdge(renderer, mesh, edge, mvp);
    }
}


void DebugRenderer::drawHalfEdge(
    IRenderer& renderer,
    const MeshData& mesh,
    EdgeHandle edgeHandle,
    const Mat4& mvp
) {
    const Edge* edge = mesh.getEdge(edgeHandle);
    if (!edge) return;

    if (!mesh.isValidHandle(edge->prev)) return;
    if (!mesh.isValidHandle(edge->tip)) return;

    const bool isBoundary =
        !mesh.isValidHandle(edge->face);

    // Boundary edges use their pair's face.
    FaceHandle referenceFace;

    if (!isBoundary) {
        referenceFace = edge->face;
    } else {
        if (!mesh.isValidHandle(edge->pair)) return;
        const Edge* pair = mesh.getEdge(edge->pair);

        if (!pair) return;
        if (!mesh.isValidHandle(pair->face)) return;

        referenceFace = pair->face;
    }

    const Edge* prev = mesh.getEdge(edge->prev);
    if (!prev) return;

    if (!mesh.isValidHandle(prev->tip)) return;

    const Vertex* prevTip = mesh.getVertex(prev->tip);
    const Vertex* edgeTip = mesh.getVertex(edge->tip);
    if (!prevTip || !edgeTip) return;

    const Vec3& startPosition = prevTip->position;
    const Vec3& endPosition = edgeTip->position;

    // Calculate reference face center

    Vec3 faceCenter(0.0f);
    u32 vertexCount = 0;

    const Face* face = mesh.getFace(referenceFace);

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

    // Offset half-edge

    constexpr f32 FACE_OFFSET = 0.08f;

    Vec3 start;
    Vec3 end;

    if (isBoundary) {
        // Move away from the pair's face.
        start = startPosition + (startPosition - faceCenter) * FACE_OFFSET;
        end = endPosition + (endPosition - faceCenter) * FACE_OFFSET;
    } else {
        // Move into this edge's face.
        start = startPosition + (faceCenter - startPosition) * FACE_OFFSET;
        end = endPosition + (faceCenter - endPosition) * FACE_OFFSET;
    }

    // Shorten the line slightly at both ends.
    constexpr f32 END_OFFSET = 0.08f;

    Vec3 direction = end - start;

    start += direction * END_OFFSET;
    end -= direction * END_OFFSET;

    Vec3 normal = mesh.getFaceNormal(referenceFace);
    
    drawArrow(
        renderer,
        start,
        end,
        normal,
        mvp
    );

    // Label

    Vec3 midpoint = (start + end) * 0.5f;
    constexpr f32 LABEL_INSET = 0.12f;
    Vec3 labelPosition;

    if (isBoundary) {
        labelPosition = midpoint + (midpoint - faceCenter) * LABEL_INSET;
    } else {
        labelPosition = midpoint + (faceCenter - midpoint) * LABEL_INSET;
    }

    Vec3 right = (end - start).normalized();
    Vec3 up = Vec3::cross(normal, right).normalized();

    if (up.y < 0.0f) {
        right *= -1.0f;
        up *= -1.0f;
    }

    std::string label = std::to_string(edgeHandle.index) + ":" + std::to_string(edgeHandle.generation);

    renderer.drawText3D(
        DrawText3DCommand{
            label,
            labelPosition,
            right,
            up,
            0.05f,
            mvp
        }
    );
}


void DebugRenderer::drawArrow(IRenderer& renderer, const Vec3& start, const Vec3& end, const Vec3& normal, const Mat4& mvp) {
    Vec3 direction = end - start;
    f32 length = direction.length();

    if (length <= 0.0001f) return;

    direction /= length;

    Vec3 side = Vec3::cross(normal, direction);

    if (side.length() <= 0.0001f) return;

    side = side.normalized();

    renderer.drawDebugLine(start, end, mvp);

    constexpr f32 ARROW_LENGTH_FACTOR = 0.08f;
    constexpr f32 ARROW_WIDTH_FACTOR  = 0.04f;

    f32 arrowLength = length * ARROW_LENGTH_FACTOR;
    f32 arrowWidth  = length * ARROW_WIDTH_FACTOR;

    Vec3 arrowBase = end - direction * arrowLength;

    Vec3 left = arrowBase + side * arrowWidth;

    Vec3 right = arrowBase - side * arrowWidth;

    renderer.drawDebugLine(end, left, mvp);
    renderer.drawDebugLine(end, right, mvp);
}


void DebugRenderer::drawHoveredElement(
    IRenderer& renderer,
    const MeshData& mesh
) {
    // TODO: implement once half-edge visualization is working.
}