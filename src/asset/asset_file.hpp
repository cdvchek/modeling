#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <types>

#include "scene/objects/object_collection.hpp"

// Valuma's side of .vlmobj assets (format: shared/vlmobj, spec: docs/systems/vlmobj.md):
// baking an object into a file, and rebuilding an object from one.
namespace AssetFile {
    inline constexpr const char* EXTENSION = ".vlmobj";

    // The GPU-ready mesh: interleaved position and normal (6 floats per vertex), shared where both match exactly
    struct BakedMesh {
        std::vector<f32> vertices;
        std::vector<u32> indices;
    };

    // Triangulates every face, flat shaded as in the viewport: a planar face's triangles use the face normal,
    // a bent face's triangles their own; then merges vertices whose position and normal are the same
    BakedMesh bake(const MeshData& mesh);

    // One object as an asset: its origin is the pivot, so the position is dropped; rotation and scale are kept
    std::vector<u8> write(const Object& object);
    // An object and everything under it: the root as above (with its world rotation and scale), then its children,
    // parents first, each with its transform relative to its parent
    std::vector<u8> write(const ObjectCollection& objects, ObjectHandle root);

    // An object read from an asset, with its parent's place in the list (NONE for the root, which comes first)
    struct ImportedObject {
        Object object;
        u32 parent = 0xFFFFFFFFu;
    };

    // Rebuilds the asset's root object: name, rotation, scale, and mesh (from the editable polygons when the file
    // has them, otherwise by joining the triangles at shared positions); the position is zero
    bool read(const std::vector<u8>& bytes, Object& object, std::string& error);
    // Every node of the asset as an object; transforms relative to the parent, as stored
    bool read(const std::vector<u8>& bytes, std::vector<ImportedObject>& objects, std::string& error);

    // To disk through a temporary file, so a failed export never leaves a damaged asset behind
    bool save(const std::filesystem::path& path, const Object& object, std::string& error);
    bool load(const std::filesystem::path& path, Object& object, std::string& error);
    bool save(const std::filesystem::path& path, const ObjectCollection& objects, ObjectHandle root, std::string& error);
    bool load(const std::filesystem::path& path, std::vector<ImportedObject>& objects, std::string& error);

    // Euler angles (radians, X then Y then Z, as Transform uses) to a unit quaternion x, y, z, w and back
    void eulerToQuaternion(const Vec3& euler, f32 out[4]);
    Vec3 quaternionToEuler(const f32 quaternion[4]);
}
