#include "project/project_file.hpp"

#include "core/io/binary_io.hpp"
#include "core/io/crc32.hpp"
#include "core/math/math_utils.hpp"
#include "core/thread/parallel_for.hpp"
#include "image/image.hpp"

#include <array>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace {
    constexpr std::array<char, 4> MAGIC = { 'V', 'L', 'M', 'S' };
    constexpr std::size_t HEADER_SIZE = 16;
    constexpr std::size_t ENTRY_SIZE = 32;
    constexpr std::size_t CHUNK_ALIGNMENT = 8;

    // Below these, starting threads costs more than it saves
    constexpr std::size_t PARALLEL_SAVE_EDGES = 20000;
    constexpr std::size_t PARALLEL_LOAD_BYTES = 1 << 20;

    constexpr u32 fourCC(const char (&name)[5]) {
        return u32(u8(name[0])) | u32(u8(name[1])) << 8 | u32(u8(name[2])) << 16 | u32(u8(name[3])) << 24;
    }

    constexpr u32 CHUNK_VIEW = fourCC("VIEW");
    constexpr u32 CHUNK_CAMERA = fourCC("CAMR");
    constexpr u32 CHUNK_LIGHTS = fourCC("LITE");
    constexpr u32 CHUNK_OBJECT = fourCC("OBJC");
    constexpr u32 CHUNK_EXPORT = fourCC("EXPT");
    constexpr u32 CHUNK_REFERENCE = fourCC("REFI");
    constexpr u32 CHUNK_MATERIALS = fourCC("MATL");
    constexpr u32 CHUNK_TEXTURE = fourCC("TEXR");

    struct ChunkVersion {
        u32 type;
        u32 version;
    };

    // The newest version of each chunk this build reads and writes
    constexpr std::array<ChunkVersion, 8> CHUNK_VERSIONS = { {
        { CHUNK_VIEW, 5 }, { CHUNK_CAMERA, 1 }, { CHUNK_LIGHTS, 1 }, { CHUNK_OBJECT, 6 }, { CHUNK_EXPORT, 1 },
        { CHUNK_REFERENCE, 1 }, { CHUNK_MATERIALS, 2 }, { CHUNK_TEXTURE, 1 },
    } };

    u32 supportedVersion(u32 type) {
        for (const ChunkVersion& chunk : CHUNK_VERSIONS) if (chunk.type == type) return chunk.version;
        return 0;
    }

    std::string typeName(u32 type) {
        std::string name;
        for (int i = 0; i < 4; ++i) {
            const char c = static_cast<char>((type >> (i * 8)) & 0xFF);
            name += (c >= 32 && c < 127) ? c : '?';
        }
        return name;
    }

    struct Chunk {
        u32 type = 0;
        u32 version = 0;
        std::vector<u8> bytes;
        u32 crc = 0;
    };

    struct Entry {
        u32 type = 0;
        u32 version = 0;
        u64 offset = 0;
        u64 size = 0;
        u32 crc = 0;
    };

    std::size_t aligned(std::size_t size) {
        return (size + CHUNK_ALIGNMENT - 1) / CHUNK_ALIGNMENT * CHUNK_ALIGNMENT;
    }

    // ---- Values ----

    void writeVec3(BinaryWriter& writer, const Vec3& value) {
        writer.write(value.x);
        writer.write(value.y);
        writer.write(value.z);
    }

    bool readVec3(BinaryReader& reader, Vec3& value) {
        return reader.read(value.x) && reader.read(value.y) && reader.read(value.z);
    }

    void writeBool(BinaryWriter& writer, bool value) {
        writer.write(static_cast<u8>(value ? 1 : 0));
    }

    bool readBool(BinaryReader& reader, bool& value) {
        u8 byte = 0;
        if (!reader.read(byte)) return false;
        value = byte != 0;
        return true;
    }

    // Modes are stored as 0 vertex, 1 edge, 2 face, 3 object, not as context bits
    constexpr std::array<u32, 4> MODES = {
        InputContext_SelectionVertex, InputContext_SelectionEdge, InputContext_SelectionFace, InputContext_SelectionObject
    };

    u32 modeIndex(u32 mode) {
        for (u32 i = 0; i < MODES.size(); ++i) if (MODES[i] == mode) return i;
        return 0;
    }

    bool readMode(BinaryReader& reader, u32& mode, u32 count) {
        u32 index = 0;
        if (!reader.read(index) || index >= count) return false;
        mode = MODES[index];
        return true;
    }

    // ---- Chunks ----

    Chunk finish(u32 type, BinaryWriter& writer) {
        Chunk chunk;
        chunk.type = type;
        chunk.version = supportedVersion(type);
        chunk.bytes = std::move(writer.bytes());
        chunk.crc = crc32(chunk.bytes.data(), chunk.bytes.size());
        return chunk;
    }

    Chunk writeView(const ProjectFile::View& view, u32 activeObject) {
        BinaryWriter writer;
        writer.write(modeIndex(view.selectionMode));
        writer.write(modeIndex(view.lastEditMode));
        writer.write(activeObject);
        writeBool(writer, view.debug);
        writeBool(writer, view.headlightEnabled);
        writeVec3(writer, view.headlightColor);
        writer.write(view.headlightStrength);
        writeVec3(writer, view.backFaceTint);
        writeBool(writer, view.showPanel);
        writer.write(view.panelRect.x);
        writer.write(view.panelRect.y);
        writer.write(view.panelRect.width);
        writer.write(view.panelRect.height);
        writer.write(view.panelTab);
        // Version 2
        writeBool(writer, view.showOrigins);
        // Version 3
        writer.write(view.exposure);
        writeBool(writer, view.showMaterials);
        // Version 4
        writeBool(writer, view.showUVChecker);
        // Version 5
        writer.write(view.workspace);
        writer.write(view.uvSplit);
        return finish(CHUNK_VIEW, writer);
    }

    bool readView(BinaryReader& reader, u32 version, ProjectFile::View& view, u32& activeObject) {
        const bool first = readMode(reader, view.selectionMode, 4)
            && readMode(reader, view.lastEditMode, 3)
            && reader.read(activeObject)
            && readBool(reader, view.debug)
            && readBool(reader, view.headlightEnabled)
            && readVec3(reader, view.headlightColor)
            && reader.read(view.headlightStrength)
            && readVec3(reader, view.backFaceTint)
            && readBool(reader, view.showPanel)
            && reader.read(view.panelRect.x)
            && reader.read(view.panelRect.y)
            && reader.read(view.panelRect.width)
            && reader.read(view.panelRect.height)
            && reader.read(view.panelTab);

        // Older versions end early and keep the defaults
        return first
            && (version < 2 || readBool(reader, view.showOrigins))
            && (version < 3 || (reader.read(view.exposure) && readBool(reader, view.showMaterials)))
            && (version < 4 || readBool(reader, view.showUVChecker))
            && (version < 5 || (reader.read(view.workspace) && view.workspace <= 1 && reader.read(view.uvSplit) && view.uvSplit >= 0.0f && view.uvSplit <= 1.0f));
    }

    Chunk writeCamera(const Camera& camera) {
        BinaryWriter writer;
        writeVec3(writer, camera.position);
        writeVec3(writer, camera.target);
        writeVec3(writer, camera.up);
        writer.write(camera.distance);
        writer.write(camera.yaw);
        writer.write(camera.pitch);
        writer.write(camera.fovRadians);
        writer.write(camera.nearPlane);
        writer.write(camera.farPlane);
        return finish(CHUNK_CAMERA, writer);
    }

    bool readCamera(BinaryReader& reader, Camera& camera) {
        return readVec3(reader, camera.position)
            && readVec3(reader, camera.target)
            && readVec3(reader, camera.up)
            && reader.read(camera.distance)
            && reader.read(camera.yaw)
            && reader.read(camera.pitch)
            && reader.read(camera.fovRadians)
            && reader.read(camera.nearPlane)
            && reader.read(camera.farPlane);
    }

    Chunk writeLights(const LightCollection& lights) {
        BinaryWriter writer;
        const AmbientLight& ambient = lights.getAmbient();
        writeVec3(writer, ambient.color);
        writer.write(ambient.strength);

        const std::vector<LightHandle> handles = lights.handles();
        writer.write(static_cast<u32>(handles.size()));

        for (LightHandle handle : handles) {
            const Light& light = lights.get(handle);
            writer.writeString(light.name);
            writer.write(static_cast<u32>(light.type));
            writeVec3(writer, light.position);
            writeVec3(writer, light.direction);
            writeVec3(writer, light.color);
            writer.write(light.intensity);
            writer.write(light.range);
            writer.write(light.innerConeRadians);
            writer.write(light.outerConeRadians);
            writeBool(writer, light.enabled);
        }

        return finish(CHUNK_LIGHTS, writer);
    }

    bool readLights(BinaryReader& reader, LightCollection& lights) {
        Vec3 ambientColor;
        f32 ambientStrength = 0.0f;
        u32 count = 0;
        if (!readVec3(reader, ambientColor) || !reader.read(ambientStrength) || !reader.read(count)) return false;

        lights.setAmbientColor(ambientColor);
        lights.setAmbientStrength(ambientStrength);

        for (u32 i = 0; i < count; ++i) {
            Light light;
            u32 type = 0;

            const bool ok = reader.readString(light.name)
                && reader.read(type) && type <= static_cast<u32>(LightType::Spot)
                && readVec3(reader, light.position)
                && readVec3(reader, light.direction)
                && readVec3(reader, light.color)
                && reader.read(light.intensity)
                && reader.read(light.range)
                && reader.read(light.innerConeRadians)
                && reader.read(light.outerConeRadians)
                && readBool(reader, light.enabled);
            if (!ok) return false;

            light.type = static_cast<LightType>(type);
            lights.add(light);
        }

        return true;
    }

    Chunk writeExport(const ProjectFile::View& view) {
        BinaryWriter writer;
        writer.writeString(view.exportFolder);
        return finish(CHUNK_EXPORT, writer);
    }

    // A picture's file as it was added: its name, size, and the PNG bytes unchanged
    void writePicture(BinaryWriter& writer, const std::shared_ptr<const Picture>& stored) {
        static const Picture EMPTY;
        const Picture& picture = stored ? *stored : EMPTY;
        writer.writeString(picture.fileName);
        writer.write(picture.width);
        writer.write(picture.height);
        writer.write(static_cast<u64>(picture.png.size()));
        writer.writeBytes(picture.png.data(), picture.png.size());
    }

    // Only checked to be a PNG of a sensible size; it's decoded when it's first drawn
    bool readPicture(BinaryReader& reader, std::shared_ptr<const Picture>& out) {
        auto picture = std::make_shared<Picture>();
        u64 size = 0;
        const bool ok = reader.readString(picture->fileName)
            && reader.read(picture->width)
            && reader.read(picture->height)
            && reader.read(size) && size <= reader.remaining()
            && reader.readVector(picture->png, static_cast<std::size_t>(size));
        if (!ok) return false;

        const bool fits = picture->width > 0 && picture->height > 0 && picture->width <= image::MAX_DIMENSION && picture->height <= image::MAX_DIMENSION;
        if (!fits || !image::isPng(picture->png.data(), picture->png.size())) return false;
        out = std::move(picture);
        return true;
    }

    // One texture: its name, the file it came from (for Reload), and its picture
    Chunk writeTexture(const Texture& texture) {
        BinaryWriter writer;
        writer.writeString(texture.name);
        writer.writeString(texture.sourcePath);
        writePicture(writer, texture.picture);
        return finish(CHUNK_TEXTURE, writer);
    }

    bool readTexture(BinaryReader& reader, Texture& texture, std::string& error) {
        if (!reader.readString(texture.name) || !reader.readString(texture.sourcePath)) {
            error = "a texture's data is cut short";
            return false;
        }
        if (!readPicture(reader, texture.picture)) {
            error = "texture '" + texture.name + "' is damaged";
            return false;
        }
        return true;
    }

    // One reference image with its picture file as it was added
    Chunk writeReference(const ReferenceImage& image) {
        BinaryWriter writer;
        writer.writeString(image.name);
        writeVec3(writer, image.position);
        writeVec3(writer, image.rotation);
        writer.write(image.size);
        writer.write(image.opacity);
        writer.write(static_cast<u8>(image.depth));
        writeBool(writer, image.locked);
        writeBool(writer, image.visible);
        writePicture(writer, image.picture);
        return finish(CHUNK_REFERENCE, writer);
    }

    bool readReference(BinaryReader& reader, ReferenceImage& image, std::string& error) {
        u8 depth = 0;

        const bool ok = reader.readString(image.name)
            && readVec3(reader, image.position)
            && readVec3(reader, image.rotation)
            && reader.read(image.size)
            && reader.read(image.opacity)
            && reader.read(depth) && depth <= static_cast<u8>(ReferenceDepth::InFront)
            && readBool(reader, image.locked)
            && readBool(reader, image.visible);

        if (!ok) {
            error = "a reference image's data is cut short or out of range";
            return false;
        }
        if (!readPicture(reader, image.picture) || !(image.size > 0.0f)) {
            error = "reference image '" + image.name + "' is damaged";
            return false;
        }

        image.depth = static_cast<ReferenceDepth>(depth);
        return true;
    }

    // Every material, Default first; each map as its texture's place among the TEXR chunks (textures, in that order)
    Chunk writeMaterials(const MaterialCollection& materials, const std::vector<TextureHandle>& textures) {
        BinaryWriter writer;
        const std::vector<MaterialHandle> handles = materials.handles();
        writer.write(static_cast<u32>(handles.size()));

        for (MaterialHandle handle : handles) {
            const Material& material = materials.get(handle);
            writer.writeString(material.name);
            writeVec3(writer, material.baseColor);
            writer.write(material.roughness);
            writer.write(material.metallic);
            writeVec3(writer, material.emissiveColor);
            writer.write(material.emissiveStrength);
            writer.write(material.opacity);
            writer.write(static_cast<u8>(material.alphaMode));
            writer.write(material.alphaCutoff);
            writeBool(writer, material.doubleSided);
            // Version 2
            u32 map = INVALID_INDEX;
            for (u32 k = 0; k < textures.size(); ++k) if (textures[k] == material.baseColorMap) map = k;
            writer.write(map);
        }

        return finish(CHUNK_MATERIALS, writer);
    }

    // The first material fills in Default; handles come back in file order, for objects to point at. maps: each
    // material's base color map as a place among the TEXR chunks (INVALID_INDEX for none), linked once all are read
    bool readMaterials(BinaryReader& reader, u32 version, MaterialCollection& materials, std::vector<MaterialHandle>& handles, std::vector<u32>& maps) {
        u32 count = 0;
        if (!reader.read(count) || count == 0) return false;

        for (u32 i = 0; i < count; ++i) {
            Material material;
            u8 mode = 0;
            const bool ok = reader.readString(material.name)
                && readVec3(reader, material.baseColor)
                && reader.read(material.roughness)
                && reader.read(material.metallic)
                && readVec3(reader, material.emissiveColor)
                && reader.read(material.emissiveStrength)
                && reader.read(material.opacity)
                && reader.read(mode) && mode <= static_cast<u8>(AlphaMode::Blend)
                && reader.read(material.alphaCutoff)
                && readBool(reader, material.doubleSided);
            if (!ok) return false;
            u32 map = INVALID_INDEX;
            if (version >= 2 && !reader.read(map)) return false;
            maps.push_back(map);
            material.alphaMode = static_cast<AlphaMode>(mode);

            if (i == 0) {
                materials.get(materials.defaultMaterial()) = material;
                handles.push_back(materials.defaultMaterial());
            } else {
                handles.push_back(materials.add(material));
            }
        }
        return true;
    }

    // parent: the parent's place among the object chunks, or INVALID_INDEX; material: its place in MATL
    // faceMaterials: each face's own material as its place in MATL (INVALID_INDEX for none), in mesh order; empty when
    // no face has one
    Chunk writeObject(const Object& object, u32 parent, u32 material, const std::vector<u32>& faceMaterials) {
        BinaryWriter writer;
        writer.writeString(object.name);
        writeVec3(writer, object.transform.position);
        writeVec3(writer, object.transform.rotation);
        writeVec3(writer, object.transform.scale);
        // Version 2
        writer.write(parent);
        // Version 3
        writer.write(material);
        object.meshData.writeTo(writer);
        // Version 4
        writer.write(static_cast<u32>(faceMaterials.size()));
        writer.writeArray(faceMaterials.data(), faceMaterials.size());
        // Version 5: every half-edge's UV (its face corner at its tip), in the mesh's edge order
        const std::vector<Vec2> uvs = object.meshData.getCornerUVs();
        writer.write(static_cast<u32>(uvs.size()));
        for (const Vec2& uv : uvs) {
            writer.write(uv.x);
            writer.write(uv.y);
        }
        // Version 6: shading, the auto angle, and every half-edge's mark (none written when nothing is marked)
        writer.write(static_cast<u8>(object.meshData.getShading()));
        writer.write(object.meshData.getSmoothAngle());
        std::vector<u8> marks;
        if (object.meshData.hasEdgeMarks()) {
            for (EdgeMark mark : object.meshData.getEdgeMarks()) marks.push_back(static_cast<u8>(mark));
        }
        writer.write(static_cast<u32>(marks.size()));
        writer.writeArray(marks.data(), marks.size());
        return finish(CHUNK_OBJECT, writer);
    }

    bool readObject(BinaryReader& reader, u32 version, Object& object, u32& parent, u32& material, std::vector<u32>& faceMaterials, std::string& error) {
        // Version 1 objects have no parent, and objects before version 3 use Default
        parent = INVALID_INDEX;
        material = 0;
        const bool ok = reader.readString(object.name)
            && readVec3(reader, object.transform.position)
            && readVec3(reader, object.transform.rotation)
            && readVec3(reader, object.transform.scale)
            && (version < 2 || reader.read(parent))
            && (version < 3 || reader.read(material))
            && object.meshData.readFrom(reader)
            && (version < 4 || [&] {
                u32 faceCount = 0;
                return reader.read(faceCount) && reader.readVector(faceMaterials, faceCount);
            }())
            && (version < 5 || [&] {
                u32 uvCount = 0;
                std::vector<f32> values;
                if (!reader.read(uvCount) || !reader.readVector(values, static_cast<std::size_t>(uvCount) * 2)) return false;
                if (uvCount != object.meshData.getEdgeHandles().size()) return false;
                std::vector<Vec2> uvs(uvCount);
                for (u32 k = 0; k < uvCount; ++k) uvs[k] = Vec2(values[k * 2], values[k * 2 + 1]);
                object.meshData.setCornerUVs(uvs);
                return true;
            }())
            && (version < 6 || [&] {
                u8 shading = 0;
                f32 angle = 0.0f;
                u32 markCount = 0;
                std::vector<u8> values;
                if (!reader.read(shading) || !reader.read(angle) || !reader.read(markCount) || !reader.readVector(values, markCount)) return false;
                if (shading > static_cast<u8>(ShadingMode::Auto) || !(angle >= 0.0f && angle <= Math::PI)) return false;
                if (markCount != 0 && markCount != object.meshData.getEdgeHandles().size()) return false;
                std::vector<EdgeMark> marks;
                for (u8 value : values) {
                    if (value > static_cast<u8>(EdgeMark::Smooth)) return false;
                    marks.push_back(static_cast<EdgeMark>(value));
                }
                object.meshData.setShading(static_cast<ShadingMode>(shading));
                object.meshData.setSmoothAngle(angle);
                if (!marks.empty()) object.meshData.setEdgeMarks(marks);
                return true;
            }());

        if (!ok) {
            error = "an object's data is cut short or out of range";
            return false;
        }

        if (!object.meshData.validate()) {
            error = "object '" + object.name + "' has a broken mesh";
            return false;
        }

        // Triangulate here, on this worker, so the first frame after opening doesn't have to
        for (FaceHandle face : object.meshData.getFaceHandles()) object.meshData.getFaceTriangles(face);
        return true;
    }

    // ---- Whole file ----

    std::vector<Chunk> buildChunks(const Scene& scene, const ProjectFile::View& view) {
        const std::vector<ObjectHandle> objects = scene.objects.handles();

        // The active object is stored as its place among the object chunks
        u32 activeObject = INVALID_INDEX;
        for (u32 i = 0; i < objects.size(); ++i) {
            if (objects[i] == scene.selection.getActiveObject()) activeObject = i;
        }

        std::vector<Chunk> chunks;
        chunks.reserve(3 + objects.size());
        chunks.push_back(writeView(view, activeObject));
        chunks.push_back(writeCamera(scene.camera));
        chunks.push_back(writeLights(scene.lights));
        chunks.push_back(writeExport(view));
        const std::vector<TextureHandle> textures = scene.textures.handles();
        for (TextureHandle handle : textures) chunks.push_back(writeTexture(scene.textures.get(handle)));
        chunks.push_back(writeMaterials(scene.materials, textures));
        for (ReferenceHandle handle : scene.references.handles()) chunks.push_back(writeReference(scene.references.get(handle)));

        std::size_t edges = 0;
        for (ObjectHandle handle : objects) edges += scene.objects.get(handle).meshData.getEdgeHandles().size();

        std::vector<Chunk> objectChunks(objects.size());
        // Parents are stored as their place in this order
        std::vector<u32> parents(objects.size(), INVALID_INDEX);
        for (u32 i = 0; i < objects.size(); ++i) {
            const ObjectHandle parent = scene.objects.parentOf(objects[i]);
            for (u32 k = 0; k < objects.size(); ++k) if (objects[k] == parent) parents[i] = k;
        }

        // Materials are stored as their place in the MATL chunk (Default is 0)
        const std::vector<MaterialHandle> materialOrder = scene.materials.handles();
        std::vector<u32> materials(objects.size(), 0);
        for (u32 i = 0; i < objects.size(); ++i) {
            const MaterialHandle material = scene.materials.resolve(scene.objects.get(objects[i]).material);
            for (u32 k = 0; k < materialOrder.size(); ++k) if (materialOrder[k] == material) materials[i] = k;
        }

        auto encode = [&](u32 i) {
            const Object& object = scene.objects.get(objects[i]);

            // Faces with a material of their own (that still exists), by its place in MATL
            std::vector<u32> faceMaterials;
            const std::vector<FaceHandle> faces = object.meshData.getFaceHandles();
            for (std::size_t f = 0; f < faces.size(); ++f) {
                const MaterialHandle own = object.meshData.getFaceMaterial(faces[f]);
                if (!scene.materials.isValid(own)) continue;
                if (faceMaterials.empty()) faceMaterials.assign(faces.size(), INVALID_INDEX);
                for (u32 k = 0; k < materialOrder.size(); ++k) if (materialOrder[k] == own) faceMaterials[f] = k;
            }

            objectChunks[i] = writeObject(object, parents[i], materials[i], faceMaterials);
        };

        if (objects.size() > 1 && edges >= PARALLEL_SAVE_EDGES) parallelFor(static_cast<u32>(objects.size()), encode);
        else for (u32 i = 0; i < objects.size(); ++i) encode(i);

        for (Chunk& chunk : objectChunks) chunks.push_back(std::move(chunk));
        return chunks;
    }

    // Header and directory, with each chunk's offset worked out from the sizes before it
    std::vector<u8> buildHead(const std::vector<Chunk>& chunks) {
        BinaryWriter directory;
        std::size_t offset = HEADER_SIZE + chunks.size() * ENTRY_SIZE;

        for (const Chunk& chunk : chunks) {
            offset = aligned(offset);
            directory.write(chunk.type);
            directory.write(chunk.version);
            directory.write(static_cast<u64>(offset));
            directory.write(static_cast<u64>(chunk.bytes.size()));
            directory.write(chunk.crc);
            directory.write(u32(0));
            offset += chunk.bytes.size();
        }

        BinaryWriter head;
        head.writeBytes(MAGIC.data(), MAGIC.size());
        head.write(ProjectFile::FORMAT_VERSION);
        head.write(static_cast<u32>(chunks.size()));
        head.write(crc32(directory.bytes().data(), directory.size()));
        head.writeBytes(directory.bytes().data(), directory.size());
        return std::move(head.bytes());
    }

    // Checks the header and directory; entries come back with offsets known to lie inside the file
    bool readDirectory(const std::vector<u8>& bytes, std::vector<Entry>& entries, std::string& error) {
        BinaryReader reader(bytes.data(), bytes.size());

        std::array<char, 4> magic {};
        u32 version = 0;
        u32 count = 0;
        u32 directoryCrc = 0;

        if (!reader.readBytes(magic.data(), magic.size()) || magic != MAGIC) {
            error = "not a Valuma Studio project";
            return false;
        }

        if (!reader.read(version) || !reader.read(count) || !reader.read(directoryCrc)) {
            error = "the file is cut short";
            return false;
        }

        if (version > ProjectFile::FORMAT_VERSION) {
            error = "made by a newer version of Valuma Studio (format " + std::to_string(version) + ")";
            return false;
        }

        if (count > reader.remaining() / ENTRY_SIZE) {
            error = "the file is cut short";
            return false;
        }

        if (crc32(bytes.data() + HEADER_SIZE, std::size_t(count) * ENTRY_SIZE) != directoryCrc) {
            error = "the chunk list is damaged";
            return false;
        }

        entries.resize(count);
        for (Entry& entry : entries) {
            u32 reserved = 0;
            reader.read(entry.type);
            reader.read(entry.version);
            reader.read(entry.offset);
            reader.read(entry.size);
            reader.read(entry.crc);
            reader.read(reserved);

            if (entry.offset > bytes.size() || entry.size > bytes.size() - entry.offset) {
                error = "chunk " + typeName(entry.type) + " runs past the end of the file";
                return false;
            }
        }

        return true;
    }

    bool checkChunk(const std::vector<u8>& bytes, const Entry& entry, std::string& error) {
        if (crc32(bytes.data() + entry.offset, entry.size) != entry.crc) {
            error = "chunk " + typeName(entry.type) + " is damaged (checksum mismatch)";
            return false;
        }
        if (entry.version > supportedVersion(entry.type)) {
            error = "chunk " + typeName(entry.type) + " is from a newer version of Valuma Studio";
            return false;
        }
        return true;
    }

    // UTF-8, so names outside the system code page don't throw
    std::string displayName(const std::filesystem::path& path) {
        const std::u8string name = path.filename().u8string();
        return std::string(name.begin(), name.end());
    }

    std::string systemError(const std::string& what, const std::filesystem::path& path, const std::error_code& code) {
        return what + " " + displayName(path) + ": " + code.message();
    }
}

std::vector<u8> ProjectFile::write(const Scene& scene, const View& view) {
    const std::vector<Chunk> chunks = buildChunks(scene, view);
    std::vector<u8> bytes = buildHead(chunks);

    for (const Chunk& chunk : chunks) {
        bytes.resize(aligned(bytes.size()), 0);
        bytes.insert(bytes.end(), chunk.bytes.begin(), chunk.bytes.end());
    }

    return bytes;
}

bool ProjectFile::read(const std::vector<u8>& bytes, Scene& scene, View& view, std::string& error) {
    std::vector<Entry> entries;
    if (!readDirectory(bytes, entries, error)) return false;

    std::vector<const Entry*> objectEntries;
    std::vector<MaterialHandle> materialHandles;
    std::vector<u32> materialMaps;
    std::vector<TextureHandle> textureHandles;
    std::size_t objectBytes = 0;
    u32 activeObject = INVALID_INDEX;

    for (const Entry& entry : entries) {
        // Chunks this version doesn't know are skipped, so newer files with extra sections still open
        if (supportedVersion(entry.type) == 0) continue;

        if (entry.type == CHUNK_OBJECT) {
            objectEntries.push_back(&entry);
            objectBytes += entry.size;
            continue;
        }

        if (!checkChunk(bytes, entry, error)) return false;

        BinaryReader reader(bytes.data() + entry.offset, entry.size);
        bool ok = true;
        if (entry.type == CHUNK_VIEW) ok = readView(reader, entry.version, view, activeObject);
        else if (entry.type == CHUNK_CAMERA) ok = readCamera(reader, scene.camera);
        else if (entry.type == CHUNK_LIGHTS) ok = readLights(reader, scene.lights);
        else if (entry.type == CHUNK_EXPORT) ok = reader.readString(view.exportFolder);
        else if (entry.type == CHUNK_MATERIALS) ok = readMaterials(reader, entry.version, scene.materials, materialHandles, materialMaps);
        else if (entry.type == CHUNK_TEXTURE) {
            Texture texture;
            if (!readTexture(reader, texture, error)) return false;
            textureHandles.push_back(scene.textures.add(std::move(texture)));
        } else if (entry.type == CHUNK_REFERENCE) {
            ReferenceImage image;
            if (!readReference(reader, image, error)) return false;
            scene.references.add(std::move(image));
        }

        if (!ok) {
            error = "chunk " + typeName(entry.type) + " is cut short or out of range";
            return false;
        }
    }

    // Maps point at textures by their place among the TEXR chunks, which may come in any order around MATL
    for (std::size_t i = 0; i < materialHandles.size() && i < materialMaps.size(); ++i) {
        if (materialMaps[i] == INVALID_INDEX) continue;
        if (materialMaps[i] >= textureHandles.size()) {
            error = "material '" + scene.materials.get(materialHandles[i]).name + "' uses a texture that isn't in the file";
            return false;
        }
        scene.materials.get(materialHandles[i]).baseColorMap = textureHandles[materialMaps[i]];
    }

    // Objects decode independently, each into its own slot, so they can run on separate threads
    const u32 count = static_cast<u32>(objectEntries.size());
    std::vector<Object> objects(count);
    std::vector<u32> parents(count, INVALID_INDEX);
    std::vector<u32> materials(count, 0);
    std::vector<std::vector<u32>> faceMaterials(count);
    std::vector<std::string> errors(count);

    auto decode = [&](u32 i) {
        const Entry& entry = *objectEntries[i];
        if (!checkChunk(bytes, entry, errors[i])) return;

        BinaryReader reader(bytes.data() + entry.offset, entry.size);
        readObject(reader, entry.version, objects[i], parents[i], materials[i], faceMaterials[i], errors[i]);
    };

    if (count > 1 && objectBytes >= PARALLEL_LOAD_BYTES) parallelFor(count, decode);
    else for (u32 i = 0; i < count; ++i) decode(i);

    for (const std::string& objectError : errors) {
        if (!objectError.empty()) {
            error = objectError;
            return false;
        }
    }

    // Every parent must be another object, and following parents up must never come back around
    for (u32 i = 0; i < count; ++i) {
        u32 up = parents[i];
        for (u32 steps = 0; up != INVALID_INDEX; ++steps) {
            if (up >= count || steps >= count) {
                error = "object '" + objects[i].name + "' has a parent that doesn't exist or loops back to it";
                return false;
            }
            up = parents[up];
        }
    }

    // A file without materials only has Default
    if (materialHandles.empty()) materialHandles.push_back(scene.materials.defaultMaterial());
    for (u32 i = 0; i < count; ++i) {
        if (materials[i] >= materialHandles.size()) {
            error = "object '" + objects[i].name + "' uses a material that doesn't exist";
            return false;
        }
        objects[i].material = materialHandles[materials[i]];

        // Faces come back in the order they were written, so the list lines up with the handles
        if (faceMaterials[i].empty()) continue;
        const std::vector<FaceHandle> faces = objects[i].meshData.getFaceHandles();
        if (faceMaterials[i].size() != faces.size()) {
            error = "object '" + objects[i].name + "' has face materials that don't match its faces";
            return false;
        }
        for (std::size_t f = 0; f < faces.size(); ++f) {
            const u32 index = faceMaterials[i][f];
            if (index == INVALID_INDEX) continue;
            if (index >= materialHandles.size()) {
                error = "object '" + objects[i].name + "' has a face using a material that doesn't exist";
                return false;
            }
            objects[i].meshData.setFaceMaterial(faces[f], materialHandles[index]);
        }
    }

    std::vector<ObjectHandle> handles(count);
    for (u32 i = 0; i < count; ++i) {
        handles[i] = scene.objects.add(std::move(objects[i]));
        if (i == activeObject) scene.selection.setActiveObject(handles[i]);
    }
    // Relative transforms were saved as they are, so the parent is linked directly
    for (u32 i = 0; i < count; ++i) {
        if (parents[i] != INVALID_INDEX) scene.objects.get(handles[i]).parent = handles[parents[i]];
    }

    return true;
}

bool ProjectFile::readFile(const std::filesystem::path& path, std::vector<u8>& bytes, std::string& error) {
    std::error_code code;
    const std::uintmax_t size = std::filesystem::file_size(path, code);
    if (code) {
        error = systemError("couldn't open", path, code);
        return false;
    }

    std::ifstream file(path, std::ios::binary);
    bytes.resize(static_cast<std::size_t>(size));
    if (!file || !file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
        error = "couldn't read " + displayName(path);
        return false;
    }

    return true;
}

bool ProjectFile::save(const std::filesystem::path& path, const Scene& scene, const View& view, std::string& error) {
    const std::vector<Chunk> chunks = buildChunks(scene, view);
    const std::vector<u8> head = buildHead(chunks);

    std::filesystem::path temporary = path;
    temporary += ".saving";

    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file) {
            error = "couldn't create " + displayName(path) + " (is the folder there, and can you write to it?)";
            return false;
        }

        static constexpr std::array<char, CHUNK_ALIGNMENT> ZEROS {};
        std::size_t written = head.size();
        file.write(reinterpret_cast<const char*>(head.data()), static_cast<std::streamsize>(head.size()));

        for (const Chunk& chunk : chunks) {
            const std::size_t padding = aligned(written) - written;
            file.write(ZEROS.data(), static_cast<std::streamsize>(padding));
            file.write(reinterpret_cast<const char*>(chunk.bytes.data()), static_cast<std::streamsize>(chunk.bytes.size()));
            written += padding + chunk.bytes.size();
        }

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
        error = systemError("couldn't replace", path, code);
        return false;
    }

    return true;
}

bool ProjectFile::load(const std::filesystem::path& path, Scene& scene, View& view, std::string& error) {
    std::vector<u8> bytes;
    return readFile(path, bytes, error) && read(bytes, scene, view, error);
}

std::string ProjectFile::storeFolder(const std::filesystem::path& folder, const std::filesystem::path& projectFile) {
    if (folder.empty()) return {};

    auto utf8 = [](const std::filesystem::path& path) {
        const std::u8string text = path.generic_u8string();
        return std::string(text.begin(), text.end());
    };

    const std::filesystem::path absolute = folder.lexically_normal();
    const std::filesystem::path base = projectFile.parent_path().lexically_normal();
    if (projectFile.empty() || absolute.root_name() != base.root_name()) return utf8(absolute);

    // Inside the project's folder, or beside it: at most one step up
    const std::filesystem::path relative = absolute.lexically_relative(base);
    u32 upSteps = 0;
    for (const std::filesystem::path& part : relative) if (part == "..") ++upSteps;
    if (relative.empty() || upSteps > 1) return utf8(absolute);
    return utf8(relative);
}

std::filesystem::path ProjectFile::resolveFolder(const std::string& stored, const std::filesystem::path& projectFile) {
    if (stored.empty()) return {};
    const std::filesystem::path path(std::u8string(stored.begin(), stored.end()));
    if (path.is_absolute() || projectFile.empty()) return path.lexically_normal();
    return (projectFile.parent_path() / path).lexically_normal();
}

std::string ProjectFile::describe(const std::vector<u8>& bytes) {
    std::ostringstream out;
    std::vector<Entry> entries;
    std::string error;

    if (!readDirectory(bytes, entries, error)) {
        out << "invalid: " << error;
        return out.str();
    }

    u32 version = 0;
    std::memcpy(&version, bytes.data() + 4, sizeof(version));
    out << "Valuma Studio project, format " << version << ", " << bytes.size() << " bytes, " << entries.size() << " chunks";

    for (const Entry& entry : entries) {
        const bool intact = crc32(bytes.data() + entry.offset, entry.size) == entry.crc;
        out << "\n" << typeName(entry.type) << " v" << entry.version
            << "  offset " << entry.offset << "  size " << entry.size
            << "  crc " << std::hex << std::setw(8) << std::setfill('0') << entry.crc << std::dec
            << (intact ? "" : " DAMAGED");

        if (!intact) continue;
        BinaryReader reader(bytes.data() + entry.offset, entry.size);

        if (entry.type == CHUNK_OBJECT) {
            std::string name;
            reader.readString(name);
            f32 transform[9] {};
            reader.readArray(transform, 9);
            u32 parent = INVALID_INDEX;
            if (entry.version >= 2) reader.read(parent);
            u32 material = 0;
            if (entry.version >= 3) reader.read(material);
            u32 counts[3] {};
            reader.readArray(counts, 3);
            out << "  '" << name << "' " << counts[0] << " vertices, " << counts[1] << " half-edges, " << counts[2] << " faces";
            if (parent != INVALID_INDEX) out << ", child of object " << parent;
            if (material != 0) out << ", material " << material;
        } else if (entry.type == CHUNK_MATERIALS) {
            MaterialCollection materials;
            std::vector<MaterialHandle> handles;
            std::vector<u32> maps;
            if (readMaterials(reader, entry.version, materials, handles, maps)) out << "  " << handles.size() << " materials";
        } else if (entry.type == CHUNK_TEXTURE) {
            Texture texture;
            std::string textureError;
            if (readTexture(reader, texture, textureError)) {
                out << "  '" << texture.name << "' " << texture.picture->fileName << ", " << texture.picture->width << " x " << texture.picture->height
                    << ", " << texture.picture->png.size() << " bytes of PNG";
            }
        } else if (entry.type == CHUNK_REFERENCE) {
            ReferenceImage image;
            std::string referenceError;
            if (readReference(reader, image, referenceError)) {
                out << "  '" << image.name << "' " << image.picture->fileName << ", " << image.picture->width << " x " << image.picture->height
                    << ", " << image.picture->png.size() << " bytes of PNG";
            }
        } else if (entry.type == CHUNK_LIGHTS) {
            f32 ambient[4] {};
            u32 count = 0;
            reader.readArray(ambient, 4);
            reader.read(count);
            out << "  " << count << " lights";
        } else if (supportedVersion(entry.type) == 0) {
            out << "  (unknown, skipped when opening)";
        }
    }

    return out.str();
}
