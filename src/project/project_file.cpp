#include "project/project_file.hpp"

#include "core/io/binary_io.hpp"
#include "core/io/crc32.hpp"
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

    struct ChunkVersion {
        u32 type;
        u32 version;
    };

    // The newest version of each chunk this build reads and writes
    constexpr std::array<ChunkVersion, 6> CHUNK_VERSIONS = { {
        { CHUNK_VIEW, 2 }, { CHUNK_CAMERA, 1 }, { CHUNK_LIGHTS, 1 }, { CHUNK_OBJECT, 2 }, { CHUNK_EXPORT, 1 },
        { CHUNK_REFERENCE, 1 },
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

        // Version 1 files end here and keep the default
        return first && (version < 2 || readBool(reader, view.showOrigins));
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

        static const ReferencePicture EMPTY;
        const ReferencePicture& picture = image.picture ? *image.picture : EMPTY;
        writer.writeString(picture.fileName);
        writer.write(picture.width);
        writer.write(picture.height);
        writer.write(static_cast<u64>(picture.png.size()));
        writer.writeBytes(picture.png.data(), picture.png.size());
        return finish(CHUNK_REFERENCE, writer);
    }

    // The picture is only checked to be a PNG here; it's decoded when it's first drawn
    bool readReference(BinaryReader& reader, ReferenceImage& image, std::string& error) {
        u8 depth = 0;
        u64 size = 0;
        auto picture = std::make_shared<ReferencePicture>();

        const bool ok = reader.readString(image.name)
            && readVec3(reader, image.position)
            && readVec3(reader, image.rotation)
            && reader.read(image.size)
            && reader.read(image.opacity)
            && reader.read(depth) && depth <= static_cast<u8>(ReferenceDepth::InFront)
            && readBool(reader, image.locked)
            && readBool(reader, image.visible)
            && reader.readString(picture->fileName)
            && reader.read(picture->width)
            && reader.read(picture->height)
            && reader.read(size) && size <= reader.remaining()
            && reader.readVector(picture->png, static_cast<std::size_t>(size));

        if (!ok) {
            error = "a reference image's data is cut short or out of range";
            return false;
        }

        const bool fits = picture->width > 0 && picture->height > 0 && picture->width <= image::MAX_DIMENSION && picture->height <= image::MAX_DIMENSION;
        if (!fits || !image::isPng(picture->png.data(), picture->png.size()) || !(image.size > 0.0f)) {
            error = "reference image '" + image.name + "' is damaged";
            return false;
        }

        image.depth = static_cast<ReferenceDepth>(depth);
        image.picture = std::move(picture);
        return true;
    }

    // parent: the parent's place among the object chunks, or INVALID_INDEX
    Chunk writeObject(const Object& object, u32 parent) {
        BinaryWriter writer;
        writer.writeString(object.name);
        writeVec3(writer, object.transform.position);
        writeVec3(writer, object.transform.rotation);
        writeVec3(writer, object.transform.scale);
        // Version 2
        writer.write(parent);
        object.meshData.writeTo(writer);
        return finish(CHUNK_OBJECT, writer);
    }

    bool readObject(BinaryReader& reader, u32 version, Object& object, u32& parent, std::string& error) {
        // Version 1 objects have no parent
        parent = INVALID_INDEX;
        const bool ok = reader.readString(object.name)
            && readVec3(reader, object.transform.position)
            && readVec3(reader, object.transform.rotation)
            && readVec3(reader, object.transform.scale)
            && (version < 2 || reader.read(parent))
            && object.meshData.readFrom(reader);

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

        auto encode = [&](u32 i) { objectChunks[i] = writeObject(scene.objects.get(objects[i]), parents[i]); };

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
        else if (entry.type == CHUNK_REFERENCE) {
            ReferenceImage image;
            if (!readReference(reader, image, error)) return false;
            scene.references.add(std::move(image));
        }

        if (!ok) {
            error = "chunk " + typeName(entry.type) + " is cut short or out of range";
            return false;
        }
    }

    // Objects decode independently, each into its own slot, so they can run on separate threads
    const u32 count = static_cast<u32>(objectEntries.size());
    std::vector<Object> objects(count);
    std::vector<u32> parents(count, INVALID_INDEX);
    std::vector<std::string> errors(count);

    auto decode = [&](u32 i) {
        const Entry& entry = *objectEntries[i];
        if (!checkChunk(bytes, entry, errors[i])) return;

        BinaryReader reader(bytes.data() + entry.offset, entry.size);
        readObject(reader, entry.version, objects[i], parents[i], errors[i]);
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
            u32 counts[3] {};
            reader.readArray(counts, 3);
            out << "  '" << name << "' " << counts[0] << " vertices, " << counts[1] << " half-edges, " << counts[2] << " faces";
            if (parent != INVALID_INDEX) out << ", child of object " << parent;
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
