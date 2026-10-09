#include "scene/objects/origin.hpp"
#include "core/math/vec4.hpp"

#include <algorithm>
#include <cfloat>

namespace {
    Vec3 transformPoint(const Mat4& matrix, const Vec3& point) {
        const Vec4 result = matrix * Vec4(point.x, point.y, point.z, 1.0f);
        return Vec3(result.x, result.y, result.z);
    }

    // The origin moved to a point given in the mesh's own space, axes unchanged
    Transform originAtLocalPoint(const ObjectCollection& objects, ObjectHandle handle, const Vec3& point) {
        Transform result = objects.worldTransform(handle);
        result.position = transformPoint(result.getMatrix(), point);
        return result;
    }
}

void localBounds(const MeshData& mesh, Vec3& low, Vec3& high) {
    low = Vec3(FLT_MAX);
    high = Vec3(-FLT_MAX);
    for (VertexHandle vertex : mesh.getVertexHandles()) {
        const Vec3 p = mesh.getVertexPosition(vertex);
        low = Vec3(std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z));
        high = Vec3(std::max(high.x, p.x), std::max(high.y, p.y), std::max(high.z, p.z));
    }
    if (low.x > high.x) low = high = Vec3(0.0f);
}

std::vector<Vec3> vertexPositions(const MeshData& mesh) {
    std::vector<Vec3> positions;
    for (VertexHandle vertex : mesh.getVertexHandles()) positions.push_back(mesh.getVertexPosition(vertex));
    return positions;
}

OriginStart captureOrigin(const ObjectCollection& objects, ObjectHandle handle) {
    OriginStart start;
    const Object* object = objects.tryGet(handle);
    if (!object) return start;

    start.world = objects.worldTransform(handle);
    start.vertices = vertexPositions(object->meshData);
    for (ObjectHandle child : objects.childrenOf(handle)) start.children.push_back({ child, objects.worldTransform(child) });
    return start;
}

void setOrigin(ObjectCollection& objects, ObjectHandle handle, const OriginStart& start, const Transform& to) {
    Object* object = objects.tryGet(handle);
    if (!object) return;

    objects.setWorldTransform(handle, to);

    // A vertex keeps its world position: new local = inverse(new world) * old world * old local
    const Mat4 change = Mat4::inverse(objects.worldMatrix(handle)) * start.world.getMatrix();
    const std::vector<VertexHandle> handles = object->meshData.getVertexHandles();
    for (std::size_t i = 0; i < handles.size() && i < start.vertices.size(); ++i) {
        object->meshData.positionVertex(handles[i], transformPoint(change, start.vertices[i]));
    }
    object->meshDirty = true;

    // Children hang off the origin, so they're put back where they were
    for (const auto& [child, world] : start.children) objects.setWorldTransform(child, world);
}

void setOrigin(ObjectCollection& objects, ObjectHandle handle, const Transform& to) {
    setOrigin(objects, handle, captureOrigin(objects, handle), to);
}

Transform originAtCenter(const ObjectCollection& objects, ObjectHandle handle) {
    Vec3 low, high;
    localBounds(objects.get(handle).meshData, low, high);
    return originAtLocalPoint(objects, handle, (low + high) * 0.5f);
}

Transform originAtBottom(const ObjectCollection& objects, ObjectHandle handle) {
    Vec3 low, high;
    localBounds(objects.get(handle).meshData, low, high);
    const Vec3 center = (low + high) * 0.5f;
    return originAtLocalPoint(objects, handle, Vec3(center.x, low.y, center.z));
}

Transform originAtVertices(const ObjectCollection& objects, ObjectHandle handle, const std::vector<VertexHandle>& vertices) {
    const MeshData& mesh = objects.get(handle).meshData;
    Vec3 sum(0.0f);
    u32 count = 0;
    for (VertexHandle vertex : vertices) {
        if (!mesh.isValidHandle(vertex)) continue;
        sum += mesh.getVertexPosition(vertex);
        ++count;
    }
    if (count == 0) return objects.worldTransform(handle);
    return originAtLocalPoint(objects, handle, sum / static_cast<f32>(count));
}

Transform originAtWorld(const ObjectCollection& objects, ObjectHandle handle) {
    Transform result = objects.worldTransform(handle);
    result.position = Vec3(0.0f);
    return result;
}

Transform originAlignedToWorld(const ObjectCollection& objects, ObjectHandle handle) {
    Transform result = objects.worldTransform(handle);
    result.rotation = Vec3(0.0f);
    return result;
}
