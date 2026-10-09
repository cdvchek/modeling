#include "test.hpp"
#include "asset/asset_file.hpp"
#include "project/project_file.hpp"
#include "scene/history.hpp"
#include "scene/scene.hpp"
#include "vlmobj/vlmobj.hpp"

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
