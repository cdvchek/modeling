#include "test.hpp"
#include "mesh_helpers.hpp"
#include "core/io/crc32.hpp"
#include "core/thread/parallel_for.hpp"
#include "project/project_file.hpp"
#include "scene/history.hpp"

#include <atomic>
#include <cstring>
#include <filesystem>

namespace {
    // A cube with a beveled corner and a removed face, so the mesh has freed slots and a border
    MeshData editedCube() {
        MeshData mesh = cube();
        SlideSession session;
        mesh.bevelVertex(mesh.getVertexHandles()[0], session);
        mesh.setSlideWidth(session, 0.3f);
        mesh.removeFace(mesh.getFaceHandles()[2]);
        return mesh;
    }

    std::vector<Vec3> positions(const MeshData& mesh) {
        std::vector<Vec3> result;
        for (VertexHandle vertex : mesh.getVertexHandles()) result.push_back(mesh.getVertexPosition(vertex));
        return result;
    }

    bool sameVec3(const Vec3& a, const Vec3& b) {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }

    // Each face's corner UVs, face by face in order
    bool sameFaceUVs(const MeshData& a, const MeshData& b) {
        const std::vector<FaceHandle> fa = a.getFaceHandles();
        const std::vector<FaceHandle> fb = b.getFaceHandles();
        if (fa.size() != fb.size()) return false;
        for (std::size_t i = 0; i < fa.size(); ++i) {
            const std::vector<Vec2> ua = a.getFaceUVs(fa[i]);
            const std::vector<Vec2> ub = b.getFaceUVs(fb[i]);
            if (ua.size() != ub.size()) return false;
            for (std::size_t k = 0; k < ua.size(); ++k) if (ua[k].x != ub[k].x || ua[k].y != ub[k].y) return false;
        }
        return true;
    }

    bool samePositions(const MeshData& a, const MeshData& b) {
        const std::vector<Vec3> pa = positions(a);
        const std::vector<Vec3> pb = positions(b);
        if (pa.size() != pb.size()) return false;
        for (std::size_t i = 0; i < pa.size(); ++i) if (!sameVec3(pa[i], pb[i])) return false;
        return true;
    }

    MeshData roundTrip(const MeshData& mesh, bool& ok) {
        BinaryWriter writer;
        mesh.writeTo(writer);

        MeshData loaded;
        BinaryReader reader(writer.bytes().data(), writer.size());
        ok = loaded.readFrom(reader) && reader.remaining() == 0;
        return loaded;
    }

    Scene sampleScene() {
        Scene scene;

        const ObjectHandle first = scene.objects.add("Cube", PresetMesh::Cube);
        Object& edited = scene.objects.get(scene.objects.add("Edited cube", PresetMesh::Cube));
        edited.meshData = editedCube();
        edited.transform.position = Vec3(1.5f, -2.0f, 0.25f);
        edited.transform.rotation = Vec3(0.1f, 0.2f, 0.3f);
        edited.transform.scale = Vec3(1.0f, 2.0f, 0.5f);
        scene.objects.add("Torus", PresetMesh::Torus);

        // A removed object leaves a gap in the slots
        scene.objects.remove(first);
        scene.selection.setActiveObject(scene.objects.handleAt(1));

        Light spot;
        spot.name = "Spot light";
        spot.type = LightType::Spot;
        spot.position = Vec3(1.0f, 2.0f, 3.0f);
        spot.direction = Vec3(0.0f, -1.0f, 0.5f);
        spot.color = Vec3(0.2f, 0.4f, 0.6f);
        spot.intensity = 0.8f;
        spot.range = 7.5f;
        spot.innerConeRadians = 0.2f;
        spot.outerConeRadians = 0.4f;
        spot.enabled = false;
        scene.lights.add(spot);
        scene.lights.add(Light { .name = "Light 2" });
        scene.lights.setAmbientColor(Vec3(0.3f, 0.2f, 0.1f));
        scene.lights.setAmbientStrength(0.25f);

        scene.camera.target = Vec3(0.5f, 0.5f, 0.0f);
        scene.camera.distance = 9.0f;
        scene.camera.yaw = 0.7f;
        scene.camera.pitch = -0.3f;
        scene.camera.updatePositionFromOrbit();
        return scene;
    }

    ProjectFile::View sampleView() {
        ProjectFile::View view;
        view.selectionMode = InputContext_SelectionObject;
        view.lastEditMode = InputContext_SelectionFace;
        view.debug = true;
        view.headlightEnabled = false;
        view.headlightColor = Vec3(0.9f, 0.8f, 0.7f);
        view.headlightStrength = 0.15f;
        view.backFaceTint = Vec3(0.95f, 0.45f, 0.7f);
        view.showPanel = false;
        view.panelRect = { 10.0f, 20.0f, 300.0f, 400.0f };
        view.panelTab = 2;
        return view;
    }

    void setU32(std::vector<u8>& bytes, std::size_t offset, u32 value) {
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    }

    u32 getU32(const std::vector<u8>& bytes, std::size_t offset) {
        u32 value = 0;
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    }

    u64 getU64(const std::vector<u8>& bytes, std::size_t offset) {
        u64 value = 0;
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    }

    // Directory entry i starts after the 16-byte header; entries are 32 bytes
    std::size_t entryAt(u32 i) { return 16 + std::size_t(i) * 32; }

    void resealDirectory(std::vector<u8>& bytes) {
        const u32 count = getU32(bytes, 8);
        setU32(bytes, 12, crc32(bytes.data() + 16, std::size_t(count) * 32));
    }

    bool readFails(const std::vector<u8>& bytes, const std::string& expected) {
        Scene scene;
        ProjectFile::View view;
        std::string error;
        return !ProjectFile::read(bytes, scene, view, error) && error.find(expected) != std::string::npos;
    }
}

TEST_CASE(crc32_matches_the_standard_check_value) {
    CHECK(crc32("123456789", 9) == 0xCBF43926u);
    CHECK(crc32("", 0) == 0u);

    // The eight-byte path agrees with byte-at-a-time for every length and offset
    const char* text = "The quick brown fox jumps over the lazy dog";
    CHECK(crc32(text, std::strlen(text)) == 0x414FA339u);
}

TEST_CASE(parallel_for_visits_each_index_once) {
    std::vector<std::atomic<u32>> visits(1000);
    parallelFor(1000, [&](u32 i) { visits[i].fetch_add(1); });

    bool once = true;
    for (const auto& count : visits) once = once && count.load() == 1;
    CHECK(once);

    u32 runs = 0;
    parallelFor(0, [&](u32) { ++runs; });
    CHECK(runs == 0);
}

TEST_CASE(mesh_round_trip_packs_freed_slots) {
    const MeshData mesh = editedCube();
    CHECK(mesh.validate());

    bool ok = false;
    const MeshData loaded = roundTrip(mesh, ok);
    CHECK(ok);
    CHECK(loaded.validate());
    CHECK(counts(loaded, mesh.getVertexHandles().size(), mesh.getEdgeHandles().size(), mesh.getFaceHandles().size()));
    CHECK(samePositions(mesh, loaded));
    CHECK(faceSides(mesh) == faceSides(loaded));
    CHECK(everyFaceTriangulates(loaded));

    // Packed: handles run 0..n-1 with no gaps
    const std::vector<EdgeHandle> edges = loaded.getEdgeHandles();
    CHECK(edges.back().index == edges.size() - 1);

    // Face loops come back in the same order and winding
    const std::vector<FaceHandle> before = mesh.getFaceHandles();
    const std::vector<FaceHandle> after = loaded.getFaceHandles();
    bool sameLoops = before.size() == after.size();
    for (std::size_t i = 0; sameLoops && i < before.size(); ++i) {
        const std::vector<VertexHandle> a = mesh.getFaceVertices(before[i]);
        const std::vector<VertexHandle> b = loaded.getFaceVertices(after[i]);
        sameLoops = a.size() == b.size();
        for (std::size_t k = 0; sameLoops && k < a.size(); ++k) sameLoops = sameVec3(mesh.getVertexPosition(a[k]), loaded.getVertexPosition(b[k]));
    }
    CHECK(sameLoops);
}

TEST_CASE(mesh_read_refuses_short_or_out_of_range_data) {
    BinaryWriter writer;
    cube().writeTo(writer);

    // Cut short
    MeshData mesh;
    BinaryReader shortReader(writer.bytes().data(), writer.size() - 4);
    CHECK(!mesh.readFrom(shortReader));

    // A face pointing at a half-edge that doesn't exist; the mesh is left alone
    std::vector<u8> bytes = writer.bytes();
    setU32(bytes, bytes.size() - 4, 999);
    MeshData kept = cube();
    BinaryReader badReader(bytes.data(), bytes.size());
    CHECK(!kept.readFrom(badReader));
    CHECK(counts(kept, 8, 24, 6));
}

TEST_CASE(project_round_trip_restores_scene_and_view) {
    const Scene scene = sampleScene();
    const ProjectFile::View view = sampleView();
    const std::vector<u8> bytes = ProjectFile::write(scene, view);

    Scene loaded;
    ProjectFile::View loadedView;
    std::string error;
    CHECK(ProjectFile::read(bytes, loaded, loadedView, error));
    CHECK(error.empty());

    // Objects in the same order, packed into slots 0..n-1
    const std::vector<ObjectHandle> before = scene.objects.handles();
    const std::vector<ObjectHandle> after = loaded.objects.handles();
    CHECK(after.size() == 2);
    CHECK(after.size() == before.size());
    for (std::size_t i = 0; i < before.size() && i < after.size(); ++i) {
        const Object& a = scene.objects.get(before[i]);
        const Object& b = loaded.objects.get(after[i]);
        CHECK(a.name == b.name);
        CHECK(sameVec3(a.transform.position, b.transform.position));
        CHECK(sameVec3(a.transform.rotation, b.transform.rotation));
        CHECK(sameVec3(a.transform.scale, b.transform.scale));
        CHECK(samePositions(a.meshData, b.meshData));
        CHECK(sameFaceUVs(a.meshData, b.meshData));
        CHECK(faceSides(a.meshData) == faceSides(b.meshData));
        CHECK(b.meshDirty);
    }

    const Object* active = loaded.activeObject();
    CHECK(active && active->name == "Edited cube");

    // Lights and ambient
    CHECK(loaded.lights.count() == 2);
    const Light& spot = loaded.lights.get(loaded.lights.handles()[0]);
    CHECK(spot.name == "Spot light");
    CHECK(spot.type == LightType::Spot);
    CHECK(sameVec3(spot.position, Vec3(1.0f, 2.0f, 3.0f)));
    CHECK(sameVec3(spot.direction, Vec3(0.0f, -1.0f, 0.5f)));
    CHECK(sameVec3(spot.color, Vec3(0.2f, 0.4f, 0.6f)));
    CHECK(spot.intensity == 0.8f && spot.range == 7.5f);
    CHECK(spot.innerConeRadians == 0.2f && spot.outerConeRadians == 0.4f);
    CHECK(!spot.enabled);
    CHECK(sameVec3(loaded.lights.getAmbient().color, Vec3(0.3f, 0.2f, 0.1f)));
    CHECK(loaded.lights.getAmbient().strength == 0.25f);

    // Camera
    CHECK(sameVec3(loaded.camera.target, scene.camera.target));
    CHECK(sameVec3(loaded.camera.position, scene.camera.position));
    CHECK(loaded.camera.distance == 9.0f && loaded.camera.yaw == 0.7f && loaded.camera.pitch == -0.3f);

    // View
    CHECK(loadedView.selectionMode == InputContext_SelectionObject);
    CHECK(loadedView.lastEditMode == InputContext_SelectionFace);
    CHECK(loadedView.debug && !loadedView.headlightEnabled && !loadedView.showPanel);
    CHECK(sameVec3(loadedView.headlightColor, view.headlightColor));
    CHECK(loadedView.headlightStrength == 0.15f);
    CHECK(sameVec3(loadedView.backFaceTint, view.backFaceTint));
    CHECK(loadedView.panelRect.x == 10.0f && loadedView.panelRect.height == 400.0f);
    CHECK(loadedView.panelTab == 2);

    // Saving what was opened gives the same bytes
    CHECK(ProjectFile::write(loaded, loadedView) == bytes);
}

TEST_CASE(project_round_trip_with_many_objects_uses_threads) {
    // Enough geometry to pass the thresholds where saving and opening split objects across threads
    Scene scene;
    for (u32 i = 0; i < 48; ++i) {
        Object& object = scene.objects.get(scene.objects.add("Torus " + std::to_string(i), PresetMesh::Torus));
        object.transform.position = Vec3(static_cast<f32>(i), 0.0f, 0.0f);
    }

    const std::vector<u8> bytes = ProjectFile::write(scene, {});
    CHECK(bytes.size() >= (1u << 20));

    Scene loaded;
    ProjectFile::View view;
    std::string error;
    CHECK(ProjectFile::read(bytes, loaded, view, error));
    CHECK(loaded.objects.count() == 48);

    bool same = true;
    const std::vector<ObjectHandle> before = scene.objects.handles();
    const std::vector<ObjectHandle> after = loaded.objects.handles();
    for (std::size_t i = 0; same && i < after.size(); ++i) {
        const Object& a = scene.objects.get(before[i]);
        const Object& b = loaded.objects.get(after[i]);
        same = a.name == b.name && a.transform.position.x == b.transform.position.x && samePositions(a.meshData, b.meshData);
    }
    CHECK(same);
}

TEST_CASE(project_read_refuses_damaged_files) {
    const std::vector<u8> bytes = ProjectFile::write(sampleScene(), sampleView());

    std::vector<u8> wrongMagic = bytes;
    wrongMagic[0] = 'X';
    CHECK(readFails(wrongMagic, "not a Valuma Studio project"));

    std::vector<u8> newer = bytes;
    setU32(newer, 4, ProjectFile::FORMAT_VERSION + 1);
    CHECK(readFails(newer, "newer version"));

    CHECK(readFails(std::vector<u8>(bytes.begin(), bytes.begin() + 10), "cut short"));

    std::vector<u8> damagedList = bytes;
    damagedList[entryAt(0) + 8] ^= 0xFF;
    CHECK(readFails(damagedList, "chunk list is damaged"));

    // Flip a byte in the last object's mesh
    std::vector<u8> damagedObject = bytes;
    const u32 count = getU32(bytes, 8);
    const u64 offset = getU64(bytes, entryAt(count - 1) + 8);
    damagedObject[offset + 40] ^= 0x01;
    CHECK(readFails(damagedObject, "OBJC is damaged"));

    // A chunk that claims to run past the end
    std::vector<u8> truncated(bytes.begin(), bytes.end() - 16);
    CHECK(readFails(truncated, "runs past the end"));
}

TEST_CASE(project_read_skips_unknown_chunks) {
    std::vector<u8> bytes = ProjectFile::write(sampleScene(), sampleView());

    // Rename the lights chunk (third) to a type this version doesn't know
    std::memcpy(bytes.data() + entryAt(2), "ZZZZ", 4);
    resealDirectory(bytes);

    Scene loaded;
    ProjectFile::View view;
    std::string error;
    CHECK(ProjectFile::read(bytes, loaded, view, error));
    CHECK(loaded.lights.count() == 0);
    CHECK(loaded.objects.count() == 2);
    CHECK(ProjectFile::describe(bytes).find("ZZZZ") != std::string::npos);
}

TEST_CASE(project_save_replaces_the_file_and_loads_back) {
    const std::filesystem::path folder = std::filesystem::temp_directory_path() / "valuma_tests";
    std::filesystem::create_directories(folder);
    const std::filesystem::path path = folder / "scene test.vlm";
    std::filesystem::remove(path);

    Scene scene = sampleScene();
    std::string error;
    CHECK(ProjectFile::save(path, scene, sampleView(), error));

    // Saving again over the same file
    scene.objects.get(scene.objects.handles()[0]).name = "Renamed";
    CHECK(ProjectFile::save(path, scene, sampleView(), error));
    CHECK(error.empty());

    std::filesystem::path leftover = path;
    leftover += ".saving";
    CHECK(!std::filesystem::exists(leftover));

    Scene loaded;
    ProjectFile::View view;
    CHECK(ProjectFile::load(path, loaded, view, error));
    CHECK(loaded.objects.get(loaded.objects.handles()[0]).name == "Renamed");

    std::vector<u8> bytes;
    CHECK(ProjectFile::readFile(path, bytes, error));
    CHECK(ProjectFile::describe(bytes).find("'Renamed' ") != std::string::npos);

    CHECK(!ProjectFile::load(folder / "missing.vlm", loaded, view, error));
    CHECK(error.find("couldn't open") != std::string::npos);

    std::filesystem::remove_all(folder);
}

TEST_CASE(history_state_id_tracks_undo_and_redo) {
    Scene scene;
    History history;
    const u64 start = history.stateId();

    history.begin(scene);
    scene.objects.add("Cube", PresetMesh::Cube);
    history.commit();
    const u64 added = history.stateId();
    CHECK(added != start);

    history.undo(scene);
    CHECK(history.stateId() == start);
    history.redo(scene);
    CHECK(history.stateId() == added);

    // A cancelled change keeps the id
    history.begin(scene);
    history.cancel(scene);
    CHECK(history.stateId() == added);

    history.clear();
    CHECK(!history.canUndo());
    CHECK(history.stateId() != added && history.stateId() != start);
}

TEST_CASE(project_export_folder_is_stored_relative_when_nearby) {
    const std::filesystem::path project = "D:/Art/rocks.vlm";

    // Inside or next to the project's folder: relative, so it moves with the project
    CHECK(ProjectFile::storeFolder("D:/Art/Exports", project) == "Exports");
    CHECK(ProjectFile::storeFolder("D:/Art/Exports/props", project) == "Exports/props");
    CHECK(ProjectFile::storeFolder("D:/Art", project) == ".");
    CHECK(ProjectFile::storeFolder("D:/Shared", project) == "../Shared");

    // Further away, another drive, or no project file yet: the full path
    CHECK(ProjectFile::storeFolder("D:/Games/Aevora/assets", "D:/Art/props/rocks.vlm") == "D:/Games/Aevora/assets");
    CHECK(ProjectFile::storeFolder("C:/Games/assets", project) == "C:/Games/assets");
    CHECK(ProjectFile::storeFolder("D:/Art/Exports", {}) == "D:/Art/Exports");
    CHECK(ProjectFile::storeFolder({}, project).empty());

    // Back again, wherever the project is now
    CHECK(ProjectFile::resolveFolder("Exports", "E:/Backup/Art/rocks.vlm") == std::filesystem::path("E:/Backup/Art/Exports"));
    CHECK(ProjectFile::resolveFolder("../Shared", project) == std::filesystem::path("D:/Shared"));
    CHECK(ProjectFile::resolveFolder("C:/Games/assets", project) == std::filesystem::path("C:/Games/assets"));
    CHECK(ProjectFile::resolveFolder("", project).empty());
}

TEST_CASE(project_saves_the_export_folder) {
    ProjectFile::View view = sampleView();
    view.exportFolder = "Exports/props";

    Scene loaded;
    ProjectFile::View loadedView;
    std::string error;
    CHECK(ProjectFile::read(ProjectFile::write(sampleScene(), view), loaded, loadedView, error));
    CHECK(loadedView.exportFolder == "Exports/props");
}

TEST_CASE(project_saves_whether_origins_show) {
    ProjectFile::View view = sampleView();
    view.showOrigins = false;

    Scene loaded;
    ProjectFile::View loadedView;
    std::string error;
    CHECK(ProjectFile::read(ProjectFile::write(sampleScene(), view), loaded, loadedView, error));
    CHECK(!loadedView.showOrigins);
}

TEST_CASE(project_saves_object_parents) {
    Scene scene = sampleScene();
    const std::vector<ObjectHandle> handles = scene.objects.handles();
    scene.objects.setParent(handles[1], handles[0]);
    const Transform world = scene.objects.worldTransform(handles[1]);

    const std::vector<u8> bytes = ProjectFile::write(scene, sampleView());
    Scene loaded;
    ProjectFile::View view;
    std::string error;
    CHECK(ProjectFile::read(bytes, loaded, view, error));

    const std::vector<ObjectHandle> loadedHandles = loaded.objects.handles();
    CHECK(loaded.objects.parentOf(loadedHandles[1]) == loadedHandles[0]);
    CHECK(loaded.objects.parentOf(loadedHandles[0]).isNull());
    const Transform back = loaded.objects.worldTransform(loadedHandles[1]);
    CHECK(sameVec3(back.position, world.position) && sameVec3(back.scale, world.scale));

    // A file whose parents loop back around is refused: point the first object at the second, which points back
    std::vector<u8> looped = bytes;
    const u32 count = getU32(bytes, 8);
    std::vector<u32> objectEntries;
    for (u32 i = 0; i < count; ++i) if (std::memcmp(bytes.data() + entryAt(i), "OBJC", 4) == 0) objectEntries.push_back(i);
    const u64 first = getU64(bytes, entryAt(objectEntries[0]) + 8);
    const u32 nameLength = getU32(bytes, first);
    setU32(looped, first + 4 + nameLength + 36, 1);
    const u64 size = getU64(bytes, entryAt(objectEntries[0]) + 16);
    setU32(looped, entryAt(objectEntries[0]) + 24, crc32(looped.data() + first, size));
    resealDirectory(looped);
    CHECK(readFails(looped, "loops back"));
}

TEST_CASE(project_saves_exposure) {
    ProjectFile::View view = sampleView();
    view.exposure = -1.5f;
    view.showMaterials = false;

    Scene loaded;
    ProjectFile::View loadedView;
    std::string error;
    CHECK(ProjectFile::read(ProjectFile::write(sampleScene(), view), loaded, loadedView, error));
    CHECK(loadedView.exposure == -1.5f);
    CHECK(!loadedView.showMaterials);
}

TEST_CASE(project_saves_materials_and_which_objects_use_them) {
    Scene scene = sampleScene();
    scene.materials.get(scene.materials.defaultMaterial()).roughness = 0.9f;

    Material glass;
    glass.name = "Glass";
    glass.baseColor = Vec3(0.6f, 0.85f, 1.0f);
    glass.roughness = 0.05f;
    glass.metallic = 0.25f;
    glass.emissiveColor = Vec3(1.0f, 0.5f, 0.0f);
    glass.emissiveStrength = 2.0f;
    glass.opacity = 0.35f;
    glass.alphaMode = AlphaMode::Blend;
    glass.alphaCutoff = 0.4f;
    glass.doubleSided = true;
    const MaterialHandle unused = scene.materials.add(Material { "Unused" });
    const MaterialHandle glassHandle = scene.materials.add(glass);
    scene.materials.remove(unused);

    const std::vector<ObjectHandle> objects = scene.objects.handles();
    scene.objects.get(objects[1]).material = glassHandle;

    ProjectFile::View view = sampleView();
    const std::vector<u8> bytes = ProjectFile::write(scene, view);

    Scene loaded;
    ProjectFile::View loadedView;
    std::string error;
    CHECK(ProjectFile::read(bytes, loaded, loadedView, error));
    CHECK(loaded.materials.count() == 2);
    CHECK(loaded.materials.get(loaded.materials.defaultMaterial()).roughness == 0.9f);

    const std::vector<ObjectHandle> loadedObjects = loaded.objects.handles();
    const MaterialHandle used = loaded.materials.resolve(loaded.objects.get(loadedObjects[1]).material);
    CHECK(!loaded.materials.isDefault(used));
    const Material& back = loaded.materials.get(used);
    CHECK(back.name == "Glass" && back.sameLook(glass));
    CHECK(loaded.materials.isDefault(loaded.materials.resolve(loaded.objects.get(loadedObjects[0]).material)));

    CHECK(ProjectFile::describe(bytes).find("MATL") != std::string::npos);
}
