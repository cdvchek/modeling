#include "asset/asset_file.hpp"

#include "scene/mesh/mesh_factory.hpp"
#include "core/math/math_utils.hpp"
#include "vlmobj/vlmobj.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

namespace {
    constexpr u32 FLOATS_PER_VERTEX = 8;

    // A vertex compared bit for bit, so only exactly equal vertices merge
    struct VertexKey {
        u32 bits[FLOATS_PER_VERTEX];
        bool operator==(const VertexKey&) const = default;
    };

    struct VertexKeyHash {
        std::size_t operator()(const VertexKey& key) const {
            u64 hash = 1469598103934665603ull;
            for (u32 value : key.bits) hash = (hash ^ value) * 1099511628211ull;
            return static_cast<std::size_t>(hash);
        }
    };

    // -0 and 0 draw the same, so they shouldn't keep vertices apart
    f32 positiveZero(f32 value) {
        return value == 0.0f ? 0.0f : value;
    }

    u64 edgeKey(u32 origin, u32 tip) {
        return (static_cast<u64>(origin) << 32) | tip;
    }

    // MeshFactory::fromPolygons expects a clean surface: no face repeating a vertex, each directed edge used once,
    // and borders that form simple closed loops. Checked here so a bad file is refused instead of building a broken mesh.
    bool polygonsAreBuildable(u32 vertexCount, const std::vector<std::vector<u32>>& faces) {
        std::unordered_set<u64> edges;
        for (const std::vector<u32>& face : faces) {
            for (std::size_t i = 0; i < face.size(); ++i) {
                if (face[i] >= vertexCount) return false;
                for (std::size_t k = i + 1; k < face.size(); ++k) if (face[i] == face[k]) return false;
                if (!edges.insert(edgeKey(face[i], face[(i + 1) % face.size()])).second) return false;
            }
        }

        // Border half-edges run opposite the unmatched face edges; each vertex may start at most one, and every
        // border edge must have one to continue into
        std::unordered_set<u32> borderStarts;
        std::vector<u32> borderEnds;
        for (u64 edge : edges) {
            const u32 origin = static_cast<u32>(edge >> 32);
            const u32 tip = static_cast<u32>(edge & 0xFFFFFFFFu);
            if (edges.contains(edgeKey(tip, origin))) continue;
            if (!borderStarts.insert(tip).second) return false;
            borderEnds.push_back(origin);
        }
        for (u32 end : borderEnds) if (!borderStarts.contains(end)) return false;
        return true;
    }

    bool buildMesh(const std::vector<Vec3>& positions, const std::vector<std::vector<u32>>& faces, MeshData& mesh, std::string& error,
                   const std::vector<std::vector<Vec2>>& faceUVs = {}, const std::vector<std::vector<EdgeMark>>& faceMarks = {}) {
        if (faces.empty()) {
            error = "the asset's mesh has no faces";
            return false;
        }
        if (!polygonsAreBuildable(static_cast<u32>(positions.size()), faces)) {
            error = "the asset's mesh isn't a surface Valuma can edit (overlapping or tangled faces)";
            return false;
        }

        MeshData built;
        built.setMesh(MeshFactory::fromPolygons(positions, faces, faceUVs, faceMarks));
        if (!built.validate()) {
            error = "the asset's mesh couldn't be rebuilt";
            return false;
        }

        mesh = std::move(built);
        return true;
    }

    // Without editable polygons: every triangle becomes a face, joined where corners share a position exactly
    bool weldTriangles(const vlmobj::File& file, const vlmobj::Mesh& mesh, MeshData& out, std::string& error) {
        const vlmobj::VertexAttribute* position = nullptr;
        const vlmobj::VertexAttribute* uv = nullptr;
        for (u32 a = 0; a < mesh.attributeCount; ++a) {
            if (mesh.attributes[a].semantic == static_cast<u8>(vlmobj::Semantic::Position)) position = &mesh.attributes[a];
            if (mesh.attributes[a].semantic == static_cast<u8>(vlmobj::Semantic::UV0) && mesh.attributes[a].format == static_cast<u8>(vlmobj::Format::F32x2)) uv = &mesh.attributes[a];
        }
        if (!position || position->format != static_cast<u8>(vlmobj::Format::F32x3)) {
            error = "the asset's mesh has no positions Valuma can read";
            return false;
        }

        const u8* vertexData = file.vertexData(mesh);
        const u8* indexData = file.indexData(mesh);

        std::vector<Vec3> positions;
        std::vector<u32> welded(mesh.vertexCount);
        std::unordered_map<VertexKey, u32, VertexKeyHash> byPosition;

        for (u32 v = 0; v < mesh.vertexCount; ++v) {
            f32 p[3];
            std::memcpy(p, vertexData + std::size_t(v) * mesh.vertexStride + position->offset, sizeof(p));

            VertexKey key {};
            for (int k = 0; k < 3; ++k) {
                p[k] = positiveZero(p[k]);
                std::memcpy(&key.bits[k], &p[k], 4);
            }

            auto [it, added] = byPosition.emplace(key, static_cast<u32>(positions.size()));
            if (added) positions.push_back(Vec3(p[0], p[1], p[2]));
            welded[v] = it->second;
        }

        auto index = [&](u32 k) {
            if (mesh.indexSize == 2) {
                u16 value = 0;
                std::memcpy(&value, indexData + std::size_t(k) * 2, 2);
                return static_cast<u32>(value);
            }
            u32 value = 0;
            std::memcpy(&value, indexData + std::size_t(k) * 4, 4);
            return value;
        };

        // Each triangle corner keeps the UV of the vertex it came from
        const auto uvOf = [&](u32 v) {
            f32 value[2] = { 0.0f, 0.0f };
            if (uv) std::memcpy(value, vertexData + std::size_t(v) * mesh.vertexStride + uv->offset, sizeof(value));
            return Vec2(value[0], value[1]);
        };

        std::vector<std::vector<u32>> faces;
        std::vector<std::vector<Vec2>> faceUVs;
        for (u32 k = 0; k + 2 < mesh.indexCount; k += 3) {
            const u32 a = welded[index(k)];
            const u32 b = welded[index(k + 1)];
            const u32 c = welded[index(k + 2)];
            // Triangles that collapse to a line or point once joined have no area to keep
            if (a == b || b == c || a == c) continue;
            faces.push_back({ a, b, c });
            faceUVs.push_back({ uvOf(index(k)), uvOf(index(k + 1)), uvOf(index(k + 2)) });
        }

        return buildMesh(positions, faces, out, error, faceUVs);
    }

    std::string displayName(const std::filesystem::path& path) {
        const std::u8string name = path.filename().u8string();
        return std::string(name.begin(), name.end());
    }
}

void AssetFile::eulerToQuaternion(const Vec3& euler, f32 out[4]) {
    // Transform turns X first, then Y, then Z: q = qz * qy * qx
    const f32 cx = std::cos(euler.x * 0.5f), sx = std::sin(euler.x * 0.5f);
    const f32 cy = std::cos(euler.y * 0.5f), sy = std::sin(euler.y * 0.5f);
    const f32 cz = std::cos(euler.z * 0.5f), sz = std::sin(euler.z * 0.5f);

    out[0] = sx * cy * cz - cx * sy * sz;
    out[1] = cx * sy * cz + sx * cy * sz;
    out[2] = cx * cy * sz - sx * sy * cz;
    out[3] = cx * cy * cz + sx * sy * sz;
}

Vec3 AssetFile::quaternionToEuler(const f32 q[4]) {
    const f32 x = q[0], y = q[1], z = q[2], w = q[3];

    // The rotation matrix entries the angles are read from (R = Rz * Ry * Rx)
    const f32 r00 = 1.0f - 2.0f * (y * y + z * z);
    const f32 r01 = 2.0f * (x * y - z * w);
    const f32 r10 = 2.0f * (x * y + z * w);
    const f32 r11 = 1.0f - 2.0f * (x * x + z * z);
    const f32 r20 = 2.0f * (x * z - y * w);
    const f32 r21 = 2.0f * (y * z + x * w);
    const f32 r22 = 1.0f - 2.0f * (x * x + y * y);

    // atan2 rather than asin, which loses precision near straight up or down
    const f32 pitch = std::atan2(-r20, std::sqrt(r00 * r00 + r10 * r10));

    // Straight up or down, X and Z turn about the same axis; put it all in Z
    if (std::fabs(r20) > 0.99999f) return Vec3(0.0f, pitch, std::atan2(-r01, r11));

    return Vec3(std::atan2(r21, r22), pitch, std::atan2(r10, r00));
}

AssetFile::BakedMesh AssetFile::bake(const MeshData& mesh, const FaceGroupOf& groupOf) {
    BakedMesh baked;
    std::unordered_map<VertexKey, u32, VertexKeyHash> shared;

    // Faces in group order (no material of their own first), keeping mesh order within a group
    std::vector<std::pair<MaterialHandle, FaceHandle>> faces;
    for (FaceHandle face : mesh.getFaceHandles()) {
        const MaterialHandle own = mesh.getFaceMaterial(face);
        faces.push_back({ groupOf ? groupOf(own) : own, face });
    }
    const auto key = [](MaterialHandle handle) {
        return handle.isNull() ? 0ull : (static_cast<u64>(handle.index) << 32 | handle.generation) + 1;
    };
    std::stable_sort(faces.begin(), faces.end(), [&](const auto& a, const auto& b) { return key(a.first) < key(b.first); });
    std::vector<f32> corners;

    for (const auto& [group, face] : faces) {
        if (baked.parts.empty() || !(baked.parts.back().material == group)) {
            baked.parts.push_back({ group, static_cast<u32>(baked.indices.size()), 0 });
        }

        // The same corners the viewport draws: position, normal (flat or smooth), UV
        corners.clear();
        mesh.appendFaceCorners(face, corners);
        for (std::size_t at = 0; at + FLOATS_PER_VERTEX <= corners.size(); at += FLOATS_PER_VERTEX) {
            f32 values[FLOATS_PER_VERTEX];
            for (u32 k = 0; k < FLOATS_PER_VERTEX; ++k) values[k] = positiveZero(corners[at + k]);

            VertexKey vertexKey;
            std::memcpy(vertexKey.bits, values, sizeof(values));

            auto [it, added] = shared.emplace(vertexKey, static_cast<u32>(baked.vertices.size() / FLOATS_PER_VERTEX));
            if (added) baked.vertices.insert(baked.vertices.end(), std::begin(values), std::end(values));
            baked.indices.push_back(it->second);
            ++baked.parts.back().indexCount;
        }
    }

    return baked;
}

namespace {
    // One node to write: its object, its parent's place in the list (NONE for the root), and its transform
    struct ExportNode {
        const Object* object;
        u32 parent;
        Transform transform;
    };

    vlmobj::EditMesh editablePolygons(const MeshData& mesh) {
        vlmobj::EditMesh editMesh;
        std::unordered_map<u32, u32> vertexIndex;
        for (VertexHandle vertex : mesh.getVertexHandles()) {
            const Vec3 position = mesh.getVertexPosition(vertex);
            vertexIndex.emplace(vertex.index, static_cast<u32>(editMesh.positions.size() / 3));
            editMesh.positions.insert(editMesh.positions.end(), { position.x, position.y, position.z });
        }
        for (FaceHandle face : mesh.getFaceHandles()) {
            const std::vector<VertexHandle> corners = mesh.getFaceVertices(face);
            editMesh.faceSizes.push_back(static_cast<u32>(corners.size()));
            for (VertexHandle corner : corners) editMesh.corners.push_back(vertexIndex.at(corner.index));
            for (const Vec2& uv : mesh.getFaceUVs(face)) editMesh.uvs.insert(editMesh.uvs.end(), { uv.x, uv.y });
            for (EdgeMark mark : mesh.getFaceEdgeMarks(face)) editMesh.edgeMarks.push_back(static_cast<u8>(mark));
        }
        editMesh.shading = static_cast<u32>(mesh.getShading());
        editMesh.smoothAngle = mesh.getSmoothAngle();
        // No marks anywhere writes none
        if (!mesh.hasEdgeMarks()) editMesh.edgeMarks.clear();
        return editMesh;
    }

    vlmobj::MaterialInput materialInput(const Material& material) {
        vlmobj::MaterialInput input;
        input.name = material.name;
        input.baseColor[0] = material.baseColor.x;
        input.baseColor[1] = material.baseColor.y;
        input.baseColor[2] = material.baseColor.z;
        input.roughness = material.roughness;
        input.metallic = material.metallic;
        input.emissiveColor[0] = material.emissiveColor.x;
        input.emissiveColor[1] = material.emissiveColor.y;
        input.emissiveColor[2] = material.emissiveColor.z;
        input.emissiveStrength = material.emissiveStrength;
        input.opacity = material.opacity;
        input.alphaCutoff = material.alphaCutoff;
        input.alphaMode = static_cast<vlmobj::AlphaMode>(material.alphaMode);
        input.doubleSided = material.doubleSided;
        return input;
    }

    Material materialFrom(const vlmobj::File& file, const vlmobj::Material& stored) {
        Material material;
        material.name = std::string(file.string(stored.name));
        if (material.name.empty()) material.name = "Material";
        material.baseColor = Vec3(stored.baseColor[0], stored.baseColor[1], stored.baseColor[2]);
        material.roughness = stored.roughness;
        material.metallic = stored.metallic;
        material.emissiveColor = Vec3(stored.emissiveColor[0], stored.emissiveColor[1], stored.emissiveColor[2]);
        material.emissiveStrength = stored.emissiveStrength;
        material.opacity = stored.opacity;
        material.alphaCutoff = stored.alphaCutoff;
        material.alphaMode = static_cast<AlphaMode>(stored.alphaMode);
        material.doubleSided = (stored.flags & vlmobj::MATERIAL_DOUBLE_SIDED) != 0;
        return material;
    }

    std::vector<u8> writeNodes(const std::vector<ExportNode>& nodes, const MaterialCollection& materials) {
        vlmobj::Writer writer;
        vlmobj::EditData edit;

        // Only the materials these objects and their faces use, in the order they're first used
        std::vector<MaterialHandle> used;
        const auto materialIndex = [&](MaterialHandle material) {
            u32 index = 0;
            while (index < used.size() && used[index] != material) ++index;
            if (index == used.size()) {
                used.push_back(material);
                writer.addMaterial(materialInput(materials.get(material)));
            }
            return index;
        };

        // A face's own material counts while it exists; otherwise the face uses its object's
        const FaceGroupOf groupOf = [&materials](MaterialHandle own) { return materials.isValid(own) ? own : INVALID_MATERIAL; };

        for (std::size_t n = 0; n < nodes.size(); ++n) {
            const ExportNode& exported = nodes[n];
            const Object& object = *exported.object;
            const AssetFile::BakedMesh baked = AssetFile::bake(object.meshData, groupOf);
            const u32 objectMaterial = materialIndex(materials.resolve(object.material));

            vlmobj::MeshInput meshInput;
            meshInput.name = object.name;
            meshInput.vertexStride = FLOATS_PER_VERTEX * sizeof(f32);
            meshInput.attributes = {
                { static_cast<u8>(vlmobj::Semantic::Position), static_cast<u8>(vlmobj::Format::F32x3), 0, 0 },
                { static_cast<u8>(vlmobj::Semantic::Normal), static_cast<u8>(vlmobj::Format::F32x3), 0, 12 },
                { static_cast<u8>(vlmobj::Semantic::UV0), static_cast<u8>(vlmobj::Format::F32x2), 0, 24 },
            };
            meshInput.vertices.resize(baked.vertices.size() * sizeof(f32));
            std::memcpy(meshInput.vertices.data(), baked.vertices.data(), meshInput.vertices.size());
            meshInput.indices = baked.indices;
            // One part per material: a face group without its own material draws in the object's
            for (const AssetFile::BakedPart& part : baked.parts) {
                const u32 material = part.material.isNull() ? objectMaterial : materialIndex(part.material);
                meshInput.parts.push_back({ part.firstIndex, part.indexCount, material });
            }

            const Transform& transform = exported.transform;
            vlmobj::NodeInput node;
            node.name = object.name;
            node.parent = exported.parent;
            node.translation[0] = transform.position.x;
            node.translation[1] = transform.position.y;
            node.translation[2] = transform.position.z;
            AssetFile::eulerToQuaternion(transform.rotation, node.rotation);
            node.scale[0] = transform.scale.x;
            node.scale[1] = transform.scale.y;
            node.scale[2] = transform.scale.z;
            node.mesh = writer.addMesh(std::move(meshInput));
            writer.addNode(node);

            // Editable polygons: vertices in mesh order, faces as corner lists; and the exact angles
            edit.eulerRotations.insert(edit.eulerRotations.end(), { transform.rotation.x, transform.rotation.y, transform.rotation.z });
            vlmobj::EditMesh editMesh = editablePolygons(object.meshData);
            editMesh.material = objectMaterial;
            const std::vector<FaceHandle> faces = object.meshData.getFaceHandles();
            for (std::size_t f = 0; f < faces.size(); ++f) {
                const MaterialHandle own = object.meshData.getFaceMaterial(faces[f]);
                if (!materials.isValid(own)) continue;
                if (editMesh.faceMaterials.empty()) editMesh.faceMaterials.assign(faces.size(), vlmobj::NONE);
                editMesh.faceMaterials[f] = materialIndex(own);
            }
            edit.meshes.push_back(std::move(editMesh));
        }

        writer.setEditData(std::move(edit));
        return writer.finish();
    }

    // The origin is the pivot: no translation; rotation and scale as the object has them in the world
    Transform pivotTransform(Transform world) {
        world.position = Vec3(0.0f);
        return world;
    }

    bool saveBytes(const std::filesystem::path& path, const std::vector<u8>& bytes, std::string& error);

    // One node into an object: name, relative transform, and mesh (editable polygons, or joined triangles)
    bool readNode(const vlmobj::File& file, const vlmobj::EditData* edit, u32 index, Object& result, std::string& error) {
        const vlmobj::Node& node = file.nodes()[index];
        result.name = std::string(file.string(node.name));
        if (result.name.empty()) result.name = "Imported";
        result.transform.position = Vec3(node.translation[0], node.translation[1], node.translation[2]);
        result.transform.scale = Vec3(node.scale[0], node.scale[1], node.scale[2]);

        // The exact angles Valuma saved, rather than ones worked back from the quaternion
        result.transform.rotation = edit
            ? Vec3(edit->eulerRotations[index * 3], edit->eulerRotations[index * 3 + 1], edit->eulerRotations[index * 3 + 2])
            : AssetFile::quaternionToEuler(node.rotation);

        result.meshDirty = true;
        if (node.mesh == vlmobj::NONE) return true;

        if (edit) {
            const vlmobj::EditMesh& editMesh = edit->meshes[node.mesh];
            std::vector<Vec3> positions;
            positions.reserve(editMesh.positions.size() / 3);
            for (std::size_t i = 0; i + 2 < editMesh.positions.size(); i += 3) {
                positions.push_back(Vec3(editMesh.positions[i], editMesh.positions[i + 1], editMesh.positions[i + 2]));
            }

            std::vector<std::vector<u32>> faces;
            std::vector<std::vector<Vec2>> faceUVs;
            std::vector<std::vector<EdgeMark>> faceMarks;
            faces.reserve(editMesh.faceSizes.size());
            const bool hasUVs = editMesh.uvs.size() == editMesh.corners.size() * 2;
            const bool hasMarks = editMesh.edgeMarks.size() == editMesh.corners.size();
            std::size_t at = 0;
            for (u32 sides : editMesh.faceSizes) {
                faces.emplace_back(editMesh.corners.begin() + at, editMesh.corners.begin() + at + sides);
                if (hasUVs) {
                    std::vector<Vec2> corners;
                    for (std::size_t k = at; k < at + sides; ++k) corners.push_back(Vec2(editMesh.uvs[k * 2], editMesh.uvs[k * 2 + 1]));
                    faceUVs.push_back(std::move(corners));
                }
                if (hasMarks) {
                    std::vector<EdgeMark> marks;
                    for (std::size_t k = at; k < at + sides; ++k) marks.push_back(static_cast<EdgeMark>(editMesh.edgeMarks[k]));
                    faceMarks.push_back(std::move(marks));
                }
                at += sides;
            }
            if (!buildMesh(positions, faces, result.meshData, error, faceUVs, faceMarks)) return false;
            result.meshData.setShading(static_cast<ShadingMode>(editMesh.shading));
            result.meshData.setSmoothAngle(editMesh.smoothAngle);
            return true;
        }

        return weldTriangles(file, file.meshes()[node.mesh], result.meshData, error);
    }
}

std::vector<u8> AssetFile::write(const Object& object, const MaterialCollection& materials) {
    return writeNodes({ { &object, vlmobj::NONE, pivotTransform(object.transform) } }, materials);
}

std::vector<u8> AssetFile::write(const ObjectCollection& objects, ObjectHandle root, const MaterialCollection& materials) {
    // The root and every object under it, parents first; each child keeps its transform relative to its parent
    std::vector<ExportNode> nodes;
    std::vector<ObjectHandle> handles;
    nodes.push_back({ &objects.get(root), vlmobj::NONE, pivotTransform(objects.worldTransform(root)) });
    handles.push_back(root);

    for (std::size_t i = 0; i < handles.size(); ++i) {
        for (ObjectHandle child : objects.childrenOf(handles[i])) {
            nodes.push_back({ &objects.get(child), static_cast<u32>(i), objects.get(child).transform });
            handles.push_back(child);
        }
    }

    return writeNodes(nodes, materials);
}

bool AssetFile::read(const std::vector<u8>& bytes, std::vector<ImportedObject>& out, std::string& error) {
    std::vector<Material> materials;
    return read(bytes, out, materials, error);
}

bool AssetFile::read(const std::vector<u8>& bytes, std::vector<ImportedObject>& out, std::vector<Material>& materialsOut, std::string& error) {
    vlmobj::File file;
    if (!file.open(bytes.data(), bytes.size())) {
        error = file.error();
        return false;
    }

    vlmobj::EditData edit;
    const bool hasEdit = file.readEditData(edit);

    std::vector<ImportedObject> result(file.nodes().size());
    for (u32 i = 0; i < result.size(); ++i) {
        if (!readNode(file, hasEdit ? &edit : nullptr, i, result[i].object, error)) return false;
        result[i].parent = file.nodes()[i].parent;

        // The object's material and its faces' own come from the editable polygons when Valuma wrote them;
        // otherwise the mesh's first part says the object's material
        const u32 mesh = file.nodes()[i].mesh;
        if (hasEdit && mesh != vlmobj::NONE && edit.meshes[mesh].material != vlmobj::NONE) {
            result[i].material = edit.meshes[mesh].material;
            result[i].faceMaterials = edit.meshes[mesh].faceMaterials;
        } else if (mesh != vlmobj::NONE && file.meshes()[mesh].partCount > 0) {
            result[i].material = file.parts()[file.meshes()[mesh].firstPart].material;
        }
    }

    std::vector<Material> materials;
    for (const vlmobj::Material& stored : file.materials()) materials.push_back(materialFrom(file, stored));

    if (result.empty() || file.nodes()[0].mesh == vlmobj::NONE) {
        error = "the asset has no mesh";
        return false;
    }

    out = std::move(result);
    materialsOut = std::move(materials);
    return true;
}

bool AssetFile::read(const std::vector<u8>& bytes, Object& object, std::string& error) {
    std::vector<ImportedObject> objects;
    if (!read(bytes, objects, error)) return false;
    object = std::move(objects[0].object);
    return true;
}

bool AssetFile::save(const std::filesystem::path& path, const ObjectCollection& objects, ObjectHandle root, const MaterialCollection& materials, std::string& error) {
    return saveBytes(path, write(objects, root, materials), error);
}

bool AssetFile::save(const std::filesystem::path& path, const Object& object, std::string& error) {
    return saveBytes(path, write(object), error);
}

namespace {
bool saveBytes(const std::filesystem::path& path, const std::vector<u8>& bytes, std::string& error) {
    std::filesystem::path temporary = path;
    temporary += ".exporting";

    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file) {
            error = "couldn't create " + displayName(path) + " (is the folder there, and can you write to it?)";
            return false;
        }

        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        file.flush();
        if (!file) {
            file.close();
            std::filesystem::remove(temporary);
            error = "couldn't finish writing " + displayName(path) + " (is the disk full?)";
            return false;
        }
    }

    std::error_code code;
    std::filesystem::rename(temporary, path, code);
    if (code) {
        std::filesystem::remove(temporary);
        error = "couldn't replace " + displayName(path) + ": " + code.message();
        return false;
    }

    return true;
}
}

bool AssetFile::load(const std::filesystem::path& path, std::vector<ImportedObject>& objects, std::string& error) {
    std::vector<Material> materials;
    return load(path, objects, materials, error);
}

bool AssetFile::load(const std::filesystem::path& path, std::vector<ImportedObject>& objects, std::vector<Material>& materials, std::string& error) {
    std::error_code code;
    const std::uintmax_t size = std::filesystem::file_size(path, code);
    if (code) {
        error = "couldn't open " + displayName(path) + ": " + code.message();
        return false;
    }

    std::vector<u8> bytes(static_cast<std::size_t>(size));
    std::ifstream file(path, std::ios::binary);
    if (!file || !file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
        error = "couldn't read " + displayName(path);
        return false;
    }

    return read(bytes, objects, materials, error);
}

bool AssetFile::load(const std::filesystem::path& path, Object& object, std::string& error) {
    std::vector<ImportedObject> objects;
    if (!load(path, objects, error)) return false;
    object = std::move(objects[0].object);
    return true;
}
