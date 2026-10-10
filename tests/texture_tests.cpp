#include "test.hpp"
#include "asset/asset_file.hpp"
#include "project/project_file.hpp"
#include "scene/history.hpp"
#include "scene/scene.hpp"
#include "scene/textures/paint_targets.hpp"
#include "image/image.hpp"
#include "vlmobj/vlmobj.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace {
    std::shared_ptr<const Picture> fixture(const char* name, u32 width, u32 height) {
        const auto path = std::filesystem::path(SOURCE_DIR) / "shared/image/tests/fixtures" / name;
        std::ifstream file(path, std::ios::binary);

        auto picture = std::make_shared<Picture>();
        picture->fileName = name;
        picture->png.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        picture->width = width;
        picture->height = height;
        return picture;
    }

    Texture texture(const std::string& name, const char* file = "rgba8.png") {
        Texture result;
        result.name = name;
        result.picture = fixture(file, 13, 7);
        result.sourcePath = "C:/paintings/" + name + ".png";
        return result;
    }

    // A scene with a cube whose material uses a texture, plus a second texture nothing uses
    Scene texturedScene(TextureHandle& used, TextureHandle& unused, MaterialHandle& material) {
        Scene scene;
        used = scene.textures.add(texture("Bark"));
        unused = scene.textures.add(texture("Moss", "gradient.png"));

        Material bark;
        bark.name = "Bark";
        bark.baseColorMap = used;
        material = scene.materials.add(bark);

        const ObjectHandle cube = scene.objects.add("Trunk", PresetMesh::Cube);
        scene.objects.get(cube).material = material;
        return scene;
    }
}

TEST_CASE(textures_are_shared_and_removal_clears_maps) {
    TextureCollection textures;
    const TextureHandle bark = textures.add(texture("Bark"));
    CHECK(textures.uniqueName("Bark") == "Bark 2");
    CHECK(textures.findSamePicture(*fixture("rgba8.png", 13, 7)) == bark);
    CHECK(!textures.isValid(textures.findSamePicture(*fixture("gradient.png", 1, 1))));

    // Two materials can point at one texture; removing it leaves their handles stale, which reads as no map
    Material a, b;
    a.baseColorMap = b.baseColorMap = bark;
    CHECK(a.sameLook(b));
    textures.remove(bark);
    CHECK(!textures.isValid(a.baseColorMap));
    CHECK(textures.count() == 0);

    // A different map makes a different look
    Material c;
    CHECK(!a.sameLook(c));
}

TEST_CASE(texture_changes_undo_and_share_pictures) {
    Scene scene;
    History history;

    history.begin(scene);
    const TextureHandle handle = scene.textures.add(texture("Bark"));
    history.commit();
    const Picture* picture = scene.textures.get(handle).picture.get();

    history.undo(scene);
    CHECK(scene.textures.count() == 0);
    history.redo(scene);
    CHECK(scene.textures.isValid(handle));
    // Undo steps hold the same picture, not copies
    CHECK(scene.textures.get(handle).picture.get() == picture);
}

TEST_CASE(project_keeps_textures_and_material_maps) {
    TextureHandle used, unused;
    MaterialHandle material;
    const Scene scene = texturedScene(used, unused, material);

    Scene loaded;
    ProjectFile::View view;
    std::string error;
    CHECK(ProjectFile::read(ProjectFile::write(scene, ProjectFile::View()), loaded, view, error));
    CHECK(error.empty());

    // Both textures come back with their names, files, and pictures byte for byte
    CHECK(loaded.textures.count() == 2);
    const TextureHandle bark = loaded.textures.handleAt(0);
    CHECK(loaded.textures.get(bark).name == "Bark");
    CHECK(loaded.textures.get(bark).sourcePath == "C:/paintings/Bark.png");
    CHECK(loaded.textures.get(bark).picture->png == scene.textures.get(used).picture->png);
    CHECK(loaded.textures.get(bark).picture->width == 13);

    // The material points at the same texture again
    MaterialHandle loadedBark = INVALID_MATERIAL;
    for (MaterialHandle handle : loaded.materials.handles()) if (loaded.materials.get(handle).name == "Bark") loadedBark = handle;
    CHECK(loaded.materials.isValid(loadedBark));
    CHECK(loaded.materials.get(loadedBark).baseColorMap == bark);
    CHECK(!loaded.textures.isValid(loaded.materials.get(loaded.materials.defaultMaterial()).baseColorMap));
}

TEST_CASE(export_embeds_only_the_textures_its_materials_use) {
    TextureHandle used, unused;
    MaterialHandle material;
    const Scene scene = texturedScene(used, unused, material);
    const ObjectHandle cube = scene.objects.handleAt(0);

    const std::vector<u8> bytes = AssetFile::write(scene.objects, cube, scene.materials, scene.textures);
    vlmobj::File file;
    CHECK(file.open(bytes.data(), bytes.size()));

    // One texture, the PNG unchanged, and the material pointing at it
    CHECK(file.textures().size() == 1);
    const vlmobj::Texture& stored = file.textures()[0];
    CHECK(file.string(stored.name) == "Bark");
    CHECK(stored.width == 13 && stored.height == 7);
    CHECK(stored.format == static_cast<u8>(vlmobj::TextureFormat::Png));
    CHECK(stored.dataOffset % vlmobj::DATA_ALIGNMENT == 0);
    const std::span<const u8> data = file.textureData(stored);
    CHECK(std::vector<u8>(data.begin(), data.end()) == scene.textures.get(used).picture->png);
    CHECK(file.materials().size() == 1);
    CHECK(file.baseColorTexture(file.materials()[0]) == 0);

    // Reading it back gives the texture and says which material uses it
    AssetFile::ImportedAsset asset;
    std::string error;
    CHECK(AssetFile::read(bytes, asset, error));
    CHECK(asset.textures.size() == 1);
    CHECK(asset.textures[0].name == "Bark");
    CHECK(asset.textures[0].sourcePath.empty());
    CHECK(asset.textures[0].picture->png == scene.textures.get(used).picture->png);
    CHECK(asset.materialMaps.size() == 1 && asset.materialMaps[0] == 0);

    // Without a map there's no texture section and the material points at none
    const Object plain = [] { Object object; object.name = "Box"; object.meshData.setMesh(PresetMesh::Cube); return object; }();
    const std::vector<u8> plainBytes = AssetFile::write(plain);
    vlmobj::File plainFile;
    CHECK(plainFile.open(plainBytes.data(), plainBytes.size()));
    CHECK(plainFile.textures().empty());
    CHECK(plainFile.find(vlmobj::Section::TEXTURES) == nullptr);
    CHECK(plainFile.baseColorTexture(plainFile.materials()[0]) == vlmobj::NONE);
}

TEST_CASE(vlmobj_refuses_bad_texture_links) {
    // A material pointing past the textures
    vlmobj::Writer writer;
    vlmobj::NodeInput node;
    node.name = "Root";
    writer.addNode(node);
    vlmobj::MaterialInput material;
    material.name = "Broken";
    material.baseColorTexture = 3;
    writer.addMaterial(material);
    const std::vector<u8> bytes = writer.finish();

    vlmobj::File file;
    CHECK(!file.open(bytes.data(), bytes.size()));
    CHECK(std::string(file.error()).find("texture") != std::string::npos);

    // A texture whose data runs past its section
    vlmobj::Writer second;
    second.addNode(node);
    second.addTexture({ "Bark", 13, 7, std::vector<u8>(64, 1) });
    std::vector<u8> damaged = second.finish();
    vlmobj::File good;
    CHECK(good.open(damaged.data(), damaged.size()));
    const vlmobj::DirectoryEntry* entry = good.find(vlmobj::Section::TEXTURES);
    CHECK(entry != nullptr);

    vlmobj::ReadOptions unchecked;
    unchecked.verifyChecksums = false;
    vlmobj::Texture record = good.textures()[0];
    record.dataSize = 1u << 20;
    std::memcpy(damaged.data() + entry->offset, &record, sizeof(record));
    vlmobj::File bad;
    CHECK(!bad.open(damaged.data(), damaged.size(), unchecked));
}

TEST_CASE(paint_targets_list_textures_with_their_materials) {
    TextureHandle used, unused;
    MaterialHandle bark;
    Scene scene = texturedScene(used, unused, bark);
    const ObjectHandle trunk = scene.objects.handles().front();
    Object& object = scene.objects.get(trunk);
    const std::vector<FaceHandle> faces = object.meshData.getFaceHandles();

    // A second material on the same texture, and one with none, each on a face of their own
    Material knot;
    knot.name = "Knot";
    knot.baseColorMap = used;
    const MaterialHandle knotHandle = scene.materials.add(knot);
    Material leaf;
    leaf.name = "Leaf";
    const MaterialHandle leafHandle = scene.materials.add(leaf);
    object.meshData.setFaceMaterial(faces[1], knotHandle);
    object.meshData.setFaceMaterial(faces[2], leafHandle);

    // The texture once with both materials, then the material without a map; the unused texture isn't listed
    const std::vector<PaintTarget> targets = paintTargets(scene, trunk);
    CHECK(targets.size() == 2);
    CHECK(targets[0].texture == used);
    CHECK(targets[0].materials.size() == 2 && targets[0].materials[0] == bark && targets[0].materials[1] == knotHandle);
    CHECK(!scene.textures.isValid(targets[1].texture));
    CHECK(targets[1].materials.size() == 1 && targets[1].materials[0] == leafHandle);

    // Only faces whose material maps the texture take its paint
    CHECK(facePaints(scene, object, faces[0], used));
    CHECK(facePaints(scene, object, faces[1], used));
    CHECK(!facePaints(scene, object, faces[2], used));
    CHECK(!facePaints(scene, object, faces[0], unused));
    CHECK(faceDrawMaterial(scene, object, faces[2]) == leafHandle);

    // No object, no targets
    CHECK(paintTargets(scene, INVALID_OBJECT).empty());
}

TEST_CASE(new_texture_layers_are_one_color) {
    // A Base layer of the color, in one shared tile
    const LayerStack layers = solidLayers(128, 64, Vec3(1.0f, 0.5f, 0.0f));
    CHECK(layers.width == 128 && layers.height == 64);
    CHECK(layers.layers.size() == 1 && layers.layers[0].name == "Base" && !layers.layers[0].fromFile);
    CHECK(layers.layers[0].tiles[0] != nullptr && layers.layers[0].tiles[0] == layers.layers[0].tiles[1]);
    CHECK(layerPixel(layers, 0, 0, 0) == (Pixel { 255, 128, 0, 255 }));
    CHECK(layerPixel(layers, 0, 127, 63) == (Pixel { 255, 128, 0, 255 }));
}

TEST_CASE(a_texture_becomes_layered_from_its_picture) {
    Texture bark = texture("Bark");
    const std::vector<u8> file = bark.picture->png;
    CHECK(!bark.layered() && bark.width() == 13 && bark.height() == 7);

    // The picture becomes the Base layer, which holds the file (for Reload) and keeps its PNG, and the picture is let go
    std::string error;
    CHECK(makeLayered(bark, error));
    CHECK(bark.layered() && bark.picture == nullptr);
    CHECK(bark.width() == 13 && bark.height() == 7);
    CHECK(bark.layers.layers.size() == 1 && bark.layers.layers[0].name == "Base" && bark.layers.layers[0].fromFile);
    CHECK(layerPng(bark.layers, 0)->bytes == file);
    image::Image decoded;
    CHECK(image::decodePng(file.data(), file.size(), decoded, error));
    CHECK(layerImage(bark.layers, 0).pixels == decoded.pixels);

    // Again changes nothing; a texture with neither is refused
    CHECK(makeLayered(bark, error));
    CHECK(bark.layers.layers.size() == 1);
    Texture empty;
    CHECK(!makeLayered(empty, error));
    CHECK(!empty.layered());
}

TEST_CASE(project_keeps_layers) {
    TextureHandle used, unused;
    MaterialHandle material;
    Scene scene = texturedScene(used, unused, material);
    std::string error;
    CHECK(makeLayered(scene.textures.get(used), error));
    LayerStack& layers = scene.textures.get(used).layers;
    const u32 shadows = addLayer(layers, "Shadows");
    fillLayer(layers, shadows, { 2, 1, 5, 3 }, { 10, 20, 30, 128 });
    layers.layers[shadows].opacity = 0.5f;
    const u32 hidden = addLayer(layers);
    fillLayer(layers, hidden, { 0, 0, 13, 7 }, { 255, 0, 255, 255 });
    layers.layers[hidden].visible = false;
    layers.active = shadows;

    const std::vector<u8> bytes = ProjectFile::write(scene, ProjectFile::View());
    Scene loaded;
    ProjectFile::View view;
    CHECK(ProjectFile::read(bytes, loaded, view, error));
    CHECK(error.empty());

    // The layered texture comes back without a picture, every layer with its settings and pixels; the other is still a picture
    const Texture& bark = loaded.textures.get(loaded.textures.handleAt(0));
    CHECK(bark.name == "Bark" && bark.sourcePath == "C:/paintings/Bark.png");
    CHECK(bark.layered() && bark.picture == nullptr && bark.width() == 13 && bark.height() == 7);
    CHECK(bark.layers.layers.size() == 3 && bark.layers.active == shadows);
    CHECK(bark.layers.layers[0].name == "Base" && bark.layers.layers[0].fromFile && bark.layers.layers[0].visible);
    CHECK(bark.layers.layers[1].name == "Shadows" && bark.layers.layers[1].opacity == 0.5f && !bark.layers.layers[1].fromFile);
    CHECK(bark.layers.layers[2].name == "Layer 1" && !bark.layers.layers[2].visible);
    for (u32 i = 0; i < 3; ++i) CHECK(layerImage(bark.layers, i).pixels == layerImage(layers, i).pixels);
    CHECK(flattenLayers(bark.layers).pixels == flattenLayers(layers).pixels);
    const Texture& moss = loaded.textures.get(loaded.textures.handleAt(1));
    CHECK(!moss.layered() && moss.picture != nullptr);

    // Saving what was loaded writes the same file: nothing is encoded again
    CHECK(ProjectFile::write(loaded, ProjectFile::View()) == bytes);

    // A layer cut short, or of another size than the texture, is refused
    Scene refused;
    std::vector<u8> cut(bytes.begin(), bytes.begin() + bytes.size() - 40);
    CHECK(!ProjectFile::read(cut, refused, view, error));
    Scene wrong = scene;
    wrong.textures.get(used).layers.width = 12;
    CHECK(!ProjectFile::read(ProjectFile::write(wrong, ProjectFile::View()), refused, view, error));
    CHECK(error.find("Base") != std::string::npos);
}

TEST_CASE(export_flattens_layers) {
    TextureHandle used, unused;
    MaterialHandle material;
    Scene scene = texturedScene(used, unused, material);
    std::string error;
    CHECK(makeLayered(scene.textures.get(used), error));
    const std::vector<u8> file = layerPng(scene.textures.get(used).layers, 0)->bytes;
    const ObjectHandle cube = scene.objects.handleAt(0);

    // With only the Base layer the file goes out as it was loaded
    std::vector<u8> bytes = AssetFile::write(scene.objects, cube, scene.materials, scene.textures);
    vlmobj::File plain;
    CHECK(plain.open(bytes.data(), bytes.size()));
    std::span<const u8> data = plain.textureData(plain.textures()[0]);
    CHECK(std::vector<u8>(data.begin(), data.end()) == file);

    // With paint over it, one PNG of the combined picture; a hidden layer isn't in it
    LayerStack& layers = scene.textures.get(used).layers;
    fillLayer(layers, addLayer(layers), { 0, 0, 6, 7 }, { 0, 255, 0, 128 });
    const u32 hidden = addLayer(layers);
    fillLayer(layers, hidden, { 0, 0, 13, 7 }, { 255, 0, 255, 255 });
    layers.layers[hidden].visible = false;

    bytes = AssetFile::write(scene.objects, cube, scene.materials, scene.textures);
    vlmobj::File painted;
    CHECK(painted.open(bytes.data(), bytes.size()));
    CHECK(painted.textures().size() == 1);
    CHECK(painted.textures()[0].width == 13 && painted.textures()[0].height == 7);
    data = painted.textureData(painted.textures()[0]);
    image::Image decoded;
    CHECK(image::decodePng(data.data(), data.size(), decoded, error));
    CHECK(decoded.pixels == flattenLayers(layers).pixels);
    CHECK(decoded.pixels != layerImage(layers, 0).pixels);
}

TEST_CASE(paint_island_keeps_to_faces_that_use_the_texture) {
    // A cylinder whose material maps a texture: the side is one island, each cap another
    Scene scene;
    const TextureHandle bark = scene.textures.add(texture("Bark"));
    Material wood;
    wood.name = "Wood";
    wood.baseColorMap = bark;
    const MaterialHandle woodHandle = scene.materials.add(wood);
    const ObjectHandle trunk = scene.objects.add("Trunk", PresetMesh::Cylinder);
    Object& object = scene.objects.get(trunk);
    object.material = woodHandle;

    std::vector<FaceHandle> sides;
    for (FaceHandle face : object.meshData.getFaceHandles()) if (object.meshData.getFaceVertices(face).size() == 4) sides.push_back(face);
    CHECK(sides.size() > 4);

    // Every side face, in handle order, and neither cap
    std::vector<FaceHandle> island = paintIsland(scene, object, sides[3], bark);
    CHECK(island.size() == sides.size());
    CHECK(std::is_sorted(island.begin(), island.end(), [](FaceHandle a, FaceHandle b) { return a.index < b.index; }));
    for (FaceHandle face : sides) CHECK(std::find(island.begin(), island.end(), face) != island.end());

    // A side face given another material drops out, and from it there's nothing to paint
    Material paintless;
    paintless.name = "Bare";
    object.meshData.setFaceMaterial(sides[0], scene.materials.add(paintless));
    island = paintIsland(scene, object, sides[3], bark);
    CHECK(island.size() == sides.size() - 1);
    CHECK(std::find(island.begin(), island.end(), sides[0]) == island.end());
    CHECK(paintIsland(scene, object, sides[0], bark).empty());
    CHECK(paintIsland(scene, object, sides[3], INVALID_TEXTURE).empty());
}

TEST_CASE(texture_axes_follow_the_surface) {
    const auto near = [](const Vec3& a, const Vec3& b) { return (a - b).length() < 1e-5f; };

    // A triangle 2 wide and 1 tall, standing up, over half of a 200 x 100 texture each way: a pixel down is a step down in the world
    const Vec3 corners[3] = { Vec3(0.0f, 1.0f, 5.0f), Vec3(2.0f, 1.0f, 5.0f), Vec3(0.0f, 0.0f, 5.0f) };
    const Vec2 uvs[3] = { Vec2(0.25f, 0.25f), Vec2(0.75f, 0.25f), Vec2(0.25f, 0.75f) };
    Vec3 perX, perY;
    CHECK(textureAxes(corners, uvs, 200, 100, perX, perY));
    CHECK(near(perX, Vec3(0.02f, 0.0f, 0.0f)));
    CHECK(near(perY, Vec3(0.0f, -0.02f, 0.0f)));

    // Any point of the triangle is its first corner plus its pixels from there along the two steps
    const Vec3 third = corners[0] + perX * ((uvs[1].x - uvs[0].x) * 200.0f) + perY * ((uvs[1].y - uvs[0].y) * 100.0f);
    CHECK(near(third, corners[1]));

    // UVs turned a quarter and mirrored still give the steps that rebuild the corners
    const Vec2 turned[3] = { Vec2(0.5f, 0.1f), Vec2(0.5f, 0.6f), Vec2(0.9f, 0.1f) };
    CHECK(textureAxes(corners, turned, 200, 100, perX, perY));
    CHECK(near(corners[0] + perX * ((turned[1].x - turned[0].x) * 200.0f) + perY * ((turned[1].y - turned[0].y) * 100.0f), corners[1]));
    CHECK(near(corners[0] + perX * ((turned[2].x - turned[0].x) * 200.0f) + perY * ((turned[2].y - turned[0].y) * 100.0f), corners[2]));

    // A triangle squashed to a line on the texture has no steps
    const Vec2 flat[3] = { Vec2(0.1f, 0.1f), Vec2(0.5f, 0.5f), Vec2(0.9f, 0.9f) };
    CHECK(!textureAxes(corners, flat, 200, 100, perX, perY));
}
