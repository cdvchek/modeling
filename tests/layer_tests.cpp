#include "test.hpp"
#include "scene/history.hpp"
#include "scene/scene.hpp"
#include "scene/textures/layers.hpp"
#include "scene/textures/paint_targets.hpp"
#include "image/image.hpp"

#include <algorithm>

namespace {
    // A picture that isn't a whole number of tiles: opaque colors that change per pixel, with a clear block from (64, 0)
    image::Image patterned(u32 width, u32 height) {
        image::Image image;
        image.width = width;
        image.height = height;
        image.pixels.resize(std::size_t(width) * height * 4);
        for (u32 y = 0; y < height; ++y) {
            for (u32 x = 0; x < width; ++x) {
                u8* p = image.pixels.data() + (std::size_t(y) * width + x) * 4;
                const bool clear = x >= 64 && x < 128 && y < 64;
                p[0] = clear ? 0 : static_cast<u8>(x * 3);
                p[1] = clear ? 0 : static_cast<u8>(y * 5);
                p[2] = clear ? 0 : static_cast<u8>(x + y);
                p[3] = clear ? 0 : 255;
            }
        }
        return image;
    }

    image::Image solid(u32 width, u32 height, const Pixel& color) {
        image::Image image;
        image.width = width;
        image.height = height;
        image.pixels.resize(std::size_t(width) * height * 4);
        for (std::size_t i = 0; i < image.pixels.size(); i += 4) std::copy(color.begin(), color.end(), image.pixels.begin() + i);
        return image;
    }

    Pixel combined(const LayerStack& stack, u32 x, u32 y) {
        Pixel pixel {};
        compositeLayers(stack, { x, y, 1, 1 }, pixel.data());
        return pixel;
    }
}

TEST_CASE(layers_from_an_image_flatten_back_to_it) {
    const image::Image image = patterned(150, 100);
    const LayerStack stack = layersFromImage(image);
    CHECK(stack.width == 150 && stack.height == 100);
    CHECK(stack.tilesAcross() == 3 && stack.tilesDown() == 2);
    CHECK(stack.layers.size() == 1 && stack.layers[0].name == "Base" && stack.active == 0);
    CHECK(stack.layers[0].tiles.size() == 6);
    CHECK(flattenLayers(stack).pixels == image.pixels);

    // The clear block takes no tile; reading it, or outside the picture, gives clear
    CHECK(stack.layers[0].tiles[1] == nullptr);
    CHECK(stack.layers[0].tiles[0] != nullptr);
    CHECK(layerPixel(stack, 0, 70, 10) == Pixel {});
    CHECK(layerPixel(stack, 0, 149, 99) == (Pixel { static_cast<u8>(149 * 3), static_cast<u8>(99 * 5), static_cast<u8>(248), 255 }));
    CHECK(layerPixel(stack, 0, 150, 0) == Pixel {});
    CHECK(layerPixel(stack, 3, 0, 0) == Pixel {});

    // Whole tiles of one color share one tile; the partial tiles at the edge get their own
    const LayerStack flat = layersFromImage(solid(200, 128, { 10, 20, 30, 255 }));
    CHECK(flat.layers[0].tiles[0] == flat.layers[0].tiles[1]);
    CHECK(flat.layers[0].tiles[0] == flat.layers[0].tiles[6]);
    CHECK(flat.layers[0].tiles[3] != flat.layers[0].tiles[0]);
    CHECK(flattenLayers(flat).pixels == solid(200, 128, { 10, 20, 30, 255 }).pixels);
}

TEST_CASE(layers_add_remove_and_reorder) {
    LayerStack stack = layersFromImage(solid(64, 64, { 1, 2, 3, 255 }));

    // New layers go above the active one, become active, and are clear
    CHECK(addLayer(stack) == 1);
    CHECK(addLayer(stack, "Shadows") == 2);
    CHECK(stack.layers.size() == 3 && stack.active == 2);
    CHECK(stack.layers[1].name == "Layer 1" && stack.layers[2].name == "Shadows");
    CHECK(stack.layers[2].tiles.size() == 1 && stack.layers[2].tiles[0] == nullptr);
    stack.active = 0;
    CHECK(addLayer(stack) == 1);
    CHECK(stack.layers[1].name == "Layer 2" && stack.layers[2].name == "Layer 1" && stack.layers[3].name == "Shadows");

    // Names count on from the highest in use, so a removed layer's name isn't given again while a later one remains
    CHECK(removeLayer(stack, 2));
    CHECK(nextLayerName(stack) == "Layer 3");

    // Moving keeps the active layer on the same layer: Base, Layer 2 (active), Shadows
    CHECK(stack.active == 1);
    CHECK(moveLayer(stack, 0, 2));
    CHECK(stack.layers[0].name == "Layer 2" && stack.layers[1].name == "Shadows" && stack.layers[2].name == "Base");
    CHECK(stack.active == 0);
    CHECK(moveLayer(stack, 0, 1));
    CHECK(stack.layers[1].name == "Layer 2" && stack.active == 1);
    CHECK(moveLayer(stack, 2, 0));
    CHECK(stack.layers[0].name == "Base" && stack.active == 2);
    CHECK(!moveLayer(stack, 0, 3));

    // Removing the active layer makes the one below active; one above the active one leaves it; the last layer stays
    CHECK(removeLayer(stack, 2));
    CHECK(stack.active == 1 && stack.layers[1].name == "Shadows");
    stack.active = 0;
    CHECK(removeLayer(stack, 1));
    CHECK(stack.active == 0 && stack.layers.size() == 1);
    CHECK(!removeLayer(stack, 0));
    CHECK(!removeLayer(stack, 5));
    CHECK(stack.layers[0].name == "Base");
}

TEST_CASE(layers_blend_in_order_by_opacity) {
    LayerStack stack = layersFromImage(solid(128, 64, { 200, 100, 0, 255 }));
    const u32 top = addLayer(stack);
    fillLayer(stack, top, { 0, 0, 10, 10 }, { 0, 0, 255, 255 });
    fillLayer(stack, top, { 20, 0, 10, 10 }, { 0, 0, 255, 128 });

    // Opaque paint covers, clear paint shows what's under it, half-clear paint mixes
    CHECK(combined(stack, 5, 5) == (Pixel { 0, 0, 255, 255 }));
    CHECK(combined(stack, 15, 5) == (Pixel { 200, 100, 0, 255 }));
    CHECK(combined(stack, 25, 5) == (Pixel { 100, 50, 128, 255 }));

    // A layer's opacity scales its paint; a hidden layer or one at 0 adds nothing
    stack.layers[top].opacity = 0.5f;
    CHECK(combined(stack, 5, 5) == (Pixel { 100, 50, 128, 255 }));
    stack.layers[top].opacity = 0.0f;
    CHECK(combined(stack, 5, 5) == (Pixel { 200, 100, 0, 255 }));
    stack.layers[top].opacity = 1.0f;
    stack.layers[top].visible = false;
    CHECK(combined(stack, 5, 5) == (Pixel { 200, 100, 0, 255 }));
    stack.layers[top].visible = true;

    // Order matters: under the opaque base, the paint is covered
    CHECK(moveLayer(stack, top, 0));
    CHECK(combined(stack, 5, 5) == (Pixel { 200, 100, 0, 255 }));
    CHECK(moveLayer(stack, 0, 1));

    // Over nothing, paint keeps its own color and alpha, and half-clear over half-clear builds up
    stack.layers[0].visible = false;
    CHECK(combined(stack, 25, 5) == (Pixel { 0, 0, 255, 128 }));
    CHECK(combined(stack, 15, 5) == Pixel {});
    stack.layers[0].visible = true;
    fillLayer(stack, 0, { 0, 0, 128, 64 }, { 255, 0, 0, 128 });
    CHECK(combined(stack, 25, 5) == (Pixel { 85, 0, 170, 192 }));

    // A part of the picture comes out the same as that part of the whole
    const image::Image whole = flattenLayers(stack);
    const PixelRect part { 3, 2, 90, 40 };
    std::vector<u8> pixels(std::size_t(part.width) * part.height * 4);
    compositeLayers(stack, part, pixels.data());
    bool same = true;
    for (u32 y = 0; same && y < part.height; ++y) {
        const u8* row = whole.pixels.data() + (std::size_t(y + part.y) * whole.width + part.x) * 4;
        same = std::equal(row, row + part.width * 4, pixels.begin() + std::size_t(y) * part.width * 4);
    }
    CHECK(same);
}

TEST_CASE(layer_fills_clip_and_share_tiles) {
    LayerStack stack = layersFromImage(solid(150, 100, { 9, 9, 9, 255 }));
    const u32 paint = addLayer(stack);

    // A fill past the edge is clipped; whole tiles of it share one tile, and the edge tiles count as whole
    fillLayer(stack, paint, { 0, 0, 500, 500 }, { 50, 60, 70, 255 });
    const std::vector<std::shared_ptr<Tile>>& tiles = stack.layers[paint].tiles;
    CHECK(tiles[0] != nullptr && tiles[0] == tiles[1] && tiles[0] == tiles[5]);
    CHECK(flattenLayers(stack).pixels == solid(150, 100, { 50, 60, 70, 255 }).pixels);

    // Filling a whole tile with clear drops it; a part of one changes only those pixels, in a tile of its own
    fillLayer(stack, paint, { 64, 0, 64, 64 }, { 0, 0, 0, 0 });
    CHECK(tiles[1] == nullptr);
    fillLayer(stack, paint, { 10, 10, 4, 4 }, { 1, 2, 3, 4 });
    CHECK(tiles[0] != tiles[2]);
    CHECK(layerPixel(stack, paint, 10, 10) == (Pixel { 1, 2, 3, 4 }));
    CHECK(layerPixel(stack, paint, 13, 13) == (Pixel { 1, 2, 3, 4 }));
    CHECK(layerPixel(stack, paint, 14, 13) == (Pixel { 50, 60, 70, 255 }));
    CHECK(layerPixel(stack, paint, 130, 70) == (Pixel { 50, 60, 70, 255 }));

    // A copy shares every tile; changing one in the copy leaves the original's pixels alone
    LayerStack copy = stack;
    CHECK(copy.layers[paint].tiles[2] == tiles[2]);
    fillLayer(copy, paint, { 130, 0, 5, 5 }, { 255, 255, 255, 255 });
    CHECK(copy.layers[paint].tiles[2] != tiles[2]);
    CHECK(copy.layers[paint].tiles[0] == tiles[0]);
    CHECK(layerPixel(stack, paint, 130, 0) == (Pixel { 50, 60, 70, 255 }));
    CHECK(layerPixel(copy, paint, 130, 0) == (Pixel { 255, 255, 255, 255 }));

    // A tile nothing else holds is changed in place
    Tile* own = copy.layers[paint].tiles[2].get();
    CHECK(&editTile(copy, paint, 2, 0) == own);

    // Nothing happens for a bad layer or an empty or outside rectangle
    fillLayer(stack, 9, { 0, 0, 10, 10 }, { 1, 1, 1, 1 });
    fillLayer(stack, paint, { 150, 0, 10, 10 }, { 1, 1, 1, 1 });
    fillLayer(stack, paint, { 0, 0, 0, 10 }, { 1, 1, 1, 1 });
    CHECK(layerPixel(stack, paint, 149, 0) == (Pixel { 50, 60, 70, 255 }));
}

TEST_CASE(texture_layers_undo_and_redo) {
    // A new texture's picture becomes its Base layer
    Scene scene;
    Texture texture;
    texture.name = "Leaf";
    auto leaf = std::make_shared<Picture>();
    leaf->fileName = "Leaf.png";
    leaf->png = image::encodePng(solid(128, 64, { 255, 128, 0, 255 }));
    leaf->width = 128;
    leaf->height = 64;
    texture.picture = leaf;
    const TextureHandle handle = scene.textures.add(texture);
    CHECK(scene.textures.get(handle).layers.empty());

    std::string error;
    History history;
    history.begin(scene);
    CHECK(layersFromPicture(*texture.picture, scene.textures.get(handle).layers, error));
    history.commit();
    CHECK(scene.textures.get(handle).layers.width == 128 && scene.textures.get(handle).layers.height == 64);
    CHECK(layerPixel(scene.textures.get(handle).layers, 0, 100, 50) == (Pixel { 255, 128, 0, 255 }));

    // A damaged picture is refused and leaves the layers alone
    Picture damaged = *texture.picture;
    damaged.png.resize(damaged.png.size() / 2);
    LayerStack untouched;
    CHECK(!layersFromPicture(damaged, untouched, error));
    CHECK(untouched.empty() && !error.empty());

    // Paint on a new layer, as one step
    history.begin(scene);
    LayerStack& layers = scene.textures.get(handle).layers;
    fillLayer(layers, addLayer(layers), { 0, 0, 8, 8 }, { 0, 255, 0, 255 });
    history.commit();
    const std::shared_ptr<Tile> painted = scene.textures.get(handle).layers.layers[1].tiles[0];
    CHECK(combined(scene.textures.get(handle).layers, 4, 4) == (Pixel { 0, 255, 0, 255 }));

    // Undo takes the layer away, redo brings back the very same tile, and undoing twice the texture is unlayered again
    CHECK(history.undo(scene));
    CHECK(scene.textures.get(handle).layers.layers.size() == 1);
    CHECK(combined(scene.textures.get(handle).layers, 4, 4) == (Pixel { 255, 128, 0, 255 }));
    CHECK(history.redo(scene));
    CHECK(scene.textures.get(handle).layers.layers[1].tiles[0] == painted);
    CHECK(scene.textures.get(handle).layers.active == 1);
    CHECK(history.undo(scene));
    CHECK(history.undo(scene));
    CHECK(scene.textures.get(handle).layers.empty());
}

TEST_CASE(layer_changes_are_found_by_tile) {
    // 150 x 100 is three tiles by two, the last column 22 wide and the bottom row 36 tall
    LayerStack shown = layersFromImage(solid(150, 100, { 9, 9, 9, 255 }));
    LayerStack now = shown;
    CHECK(changedRects(shown, now).empty());

    // An empty new layer changes nothing; paint on it changes its tile, clipped at the picture's edge
    const u32 paint = addLayer(now);
    CHECK(changedRects(shown, now).empty());
    fillLayer(now, paint, { 140, 70, 4, 4 }, { 255, 0, 0, 255 });
    std::vector<PixelRect> rects = changedRects(shown, now);
    CHECK(rects.size() == 1);
    CHECK(rects[0].x == 128 && rects[0].y == 64 && rects[0].width == 22 && rects[0].height == 36);

    // Changed tiles side by side are one rectangle; another row is another
    fillLayer(now, paint, { 60, 10, 10, 10 }, { 0, 255, 0, 255 });
    rects = changedRects(shown, now);
    CHECK(rects.size() == 2);
    CHECK(rects[0].x == 0 && rects[0].y == 0 && rects[0].width == 128 && rects[0].height == 64);
    CHECK(rects[1].x == 128 && rects[1].y == 64);
    shown = now;

    // Hiding or fading a layer changes only where it has paint, and showing it again is back to the same
    now.layers[paint].visible = false;
    CHECK(changedRects(shown, now).size() == 2);
    now.layers[paint].visible = true;
    CHECK(changedRects(shown, now).empty());
    now.layers[paint].opacity = 0.5f;
    CHECK(changedRects(shown, now).size() == 2);
    now.layers[paint].opacity = 1.0f;

    // Reordering changes where both layers show; removing a layer, where it had paint; a rename, nothing
    CHECK(moveLayer(now, paint, 0));
    CHECK(changedRects(shown, now).size() == 2);
    CHECK(moveLayer(now, 0, paint));
    now.layers[paint].name = "Rust";
    CHECK(changedRects(shown, now).empty());
    CHECK(removeLayer(now, paint));
    CHECK(changedRects(shown, now).size() == 2);

    // A tile repainted with the same pixels still counts (tiles are compared, not pixels), and another size is everything
    now = shown;
    fillLayer(now, paint, { 140, 70, 4, 4 }, { 255, 0, 0, 255 });
    CHECK(changedRects(shown, now).size() == 1);
    rects = changedRects(shown, layersFromImage(solid(70, 30, { 1, 1, 1, 255 })));
    CHECK(rects.size() == 1 && rects[0].x == 0 && rects[0].y == 0 && rects[0].width == 70 && rects[0].height == 30);
    CHECK(changedRects(shown, LayerStack {}).empty());

    // Sending just the changed rectangles keeps a copy of the picture the same as the whole
    image::Image screen = flattenLayers(shown);
    fillLayer(now, 0, { 0, 80, 150, 5 }, { 0, 0, 255, 128 });
    for (const PixelRect& rect : changedRects(shown, now)) {
        std::vector<u8> pixels(std::size_t(rect.width) * rect.height * 4);
        compositeLayers(now, rect, pixels.data());
        for (u32 y = 0; y < rect.height; ++y) {
            std::copy(pixels.begin() + std::size_t(y) * rect.width * 4, pixels.begin() + std::size_t(y + 1) * rect.width * 4,
                      screen.pixels.begin() + (std::size_t(y + rect.y) * screen.width + rect.x) * 4);
        }
    }
    CHECK(screen.pixels == flattenLayers(now).pixels);
}

TEST_CASE(layers_keep_their_png_until_they_change) {
    LayerStack stack = layersFromImage(patterned(150, 100));
    const u32 paint = addLayer(stack);
    fillLayer(stack, paint, { 10, 10, 30, 30 }, { 1, 2, 3, 200 });

    // A layer's PNG holds its own pixels, and asking again gives the same one
    const std::shared_ptr<const LayerPng> first = layerPng(stack, paint);
    image::Image decoded;
    std::string error;
    CHECK(image::decodePng(first->bytes.data(), first->bytes.size(), decoded, error));
    CHECK(decoded.width == 150 && decoded.height == 100);
    CHECK(decoded.pixels == layerImage(stack, paint).pixels);
    CHECK(layerImage(stack, paint).pixels != flattenLayers(stack).pixels);
    CHECK(layerPng(stack, paint) == first);

    // A copy (an undo step) shares it; settings don't touch it; paint does, in the changed stack only
    LayerStack copy = stack;
    copy.layers[paint].opacity = 0.5f;
    copy.layers[paint].name = "Rust";
    CHECK(layerPng(copy, paint) == first);
    fillLayer(copy, paint, { 0, 0, 5, 5 }, { 9, 9, 9, 255 });
    const std::shared_ptr<const LayerPng> second = layerPng(copy, paint);
    CHECK(second != first);
    CHECK(layerPng(stack, paint) == first);
    CHECK(image::decodePng(second->bytes.data(), second->bytes.size(), decoded, error));
    CHECK(decoded.pixels == layerImage(copy, paint).pixels);

    // A PNG of the stack's size replaces a layer's pixels and is kept as its PNG; another size is refused and changes nothing
    const std::vector<u8> red = image::encodePng(solid(150, 100, { 255, 0, 0, 255 }));
    CHECK(setLayerPng(stack, paint, red, error));
    CHECK(layerPixel(stack, paint, 149, 99) == (Pixel { 255, 0, 0, 255 }));
    CHECK(layerPng(stack, paint)->bytes == red);
    CHECK(!setLayerPng(stack, paint, image::encodePng(solid(64, 64, { 0, 0, 0, 255 })), error));
    CHECK(error.find("64 x 64") != std::string::npos);
    CHECK(!setLayerPng(stack, 7, red, error));
    CHECK(layerPixel(stack, paint, 0, 0) == (Pixel { 255, 0, 0, 255 }));

    // The combined picture as a PNG: one showing layer at full opacity is its own PNG, anything else is flattened
    stack.layers[0].visible = false;
    CHECK(flattenedPng(stack) == red);
    stack.layers[paint].opacity = 0.5f;
    const std::vector<u8> faded = flattenedPng(stack);
    CHECK(faded != red);
    CHECK(image::decodePng(faded.data(), faded.size(), decoded, error));
    CHECK(decoded.pixels == flattenLayers(stack).pixels);
}
