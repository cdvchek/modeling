#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <types>

#include "scene/objects/object_collection.hpp"
#include "scene/materials/material_collection.hpp"

// Valuma's side of .vlmobj assets (format: shared/vlmobj, spec: docs/systems/vlmobj.md):
// baking an object into a file, and rebuilding an object from one.
namespace AssetFile {
    inline constexpr const char* EXTENSION = ".vlmobj";

    // A run of indices drawn with one material (INVALID_MATERIAL: the object's)
    struct BakedPart {
        MaterialHandle material = INVALID_MATERIAL;
        u32 firstIndex = 0;
        u32 indexCount = 0;
    };

    // The GPU-ready mesh: interleaved position and normal (6 floats per vertex), shared where both match exactly,
    // with the triangles in one run per material
    struct BakedMesh {
        std::vector<f32> vertices;
        std::vector<u32> indices;
        std::vector<BakedPart> parts;
    };

    // Triangulates every face, flat shaded as in the viewport: a planar face's triangles use the face normal,
    // a bent face's triangles their own; then merges vertices whose position and normal are the same.
    // Faces are grouped by groupOf (their own material handle when not given), each group one part.
    BakedMesh bake(const MeshData& mesh, const FaceGroupOf& groupOf = {});

    // One object as an asset: its origin is the pivot, so the position is dropped; rotation and scale are kept.
    // The materials its objects use are embedded (only those), each object's mesh pointing at its own.
    std::vector<u8> write(const Object& object, const MaterialCollection& materials = MaterialCollection());
    // An object and everything under it: the root as above (with its world rotation and scale), then its children,
    // parents first, each with its transform relative to its parent
    std::vector<u8> write(const ObjectCollection& objects, ObjectHandle root, const MaterialCollection& materials = MaterialCollection());

    // An object read from an asset, with its parent's place in the list (NONE for the root, which comes first) and
    // its material's place in the asset's materials (NONE for none: the project's Default)
    struct ImportedObject {
        Object object;
        u32 parent = 0xFFFFFFFFu;
        u32 material = 0xFFFFFFFFu;
        // Each face's own material, in face order, as places in the asset's materials (NONE: the object's); empty
        // when no face has one
        std::vector<u32> faceMaterials;
    };

    // Rebuilds the asset's root object: name, rotation, scale, and mesh (from the editable polygons when the file
    // has them, otherwise by joining the triangles at shared positions); the position is zero
    bool read(const std::vector<u8>& bytes, Object& object, std::string& error);
    // Every node of the asset as an object; transforms relative to the parent, as stored
    bool read(const std::vector<u8>& bytes, std::vector<ImportedObject>& objects, std::string& error);
    // Also the asset's materials, for the objects' material places to point into
    bool read(const std::vector<u8>& bytes, std::vector<ImportedObject>& objects, std::vector<Material>& materials, std::string& error);

    // To disk through a temporary file, so a failed export never leaves a damaged asset behind
    bool save(const std::filesystem::path& path, const Object& object, std::string& error);
    bool load(const std::filesystem::path& path, Object& object, std::string& error);
    bool save(const std::filesystem::path& path, const ObjectCollection& objects, ObjectHandle root, const MaterialCollection& materials, std::string& error);
    bool load(const std::filesystem::path& path, std::vector<ImportedObject>& objects, std::string& error);
    bool load(const std::filesystem::path& path, std::vector<ImportedObject>& objects, std::vector<Material>& materials, std::string& error);

    // Euler angles (radians, X then Y then Z, as Transform uses) to a unit quaternion x, y, z, w and back
    void eulerToQuaternion(const Vec3& euler, f32 out[4]);
    Vec3 quaternionToEuler(const f32 quaternion[4]);
}
