#include "test.hpp"
#include "scene/history.hpp"
#include "scene/scene.hpp"
#include "scene/textures/brush.hpp"
#include "scene/textures/paint_targets.hpp"

#include <algorithm>

namespace {
    // A 256 x 256 stack with a clear layer (1) over an opaque white Base (0)
    LayerStack canvas() {
        LayerStack stack = solidLayers(256, 256, Vec3(1.0f, 1.0f, 1.0f));
        addLayer(stack);
        return stack;
    }

    Brush brush(f32 size, f32 softness = 0.0f, f32 opacity = 1.0f) {
        Brush result;
        result.color = Vec3(1.0f, 0.0f, 0.0f);
        result.size = size;
        result.softness = softness;
        result.opacity = opacity;
        return result;
    }

    u8 alphaAt(const LayerStack& stack, u32 layer, u32 x, u32 y) {
        return layerPixel(stack, layer, x, y)[3];
    }
}

TEST_CASE(brush_dab_is_a_round_stamp) {
    LayerStack stack = canvas();
    const LayerStack before = stack;
    Stroke stroke;
    stroke.begin(stack, 1, brush(20.0f));
    CHECK(stroke.active() && !stroke.painted());
    CHECK(stroke.moveTo(stack, Vec2(100.5f, 100.5f)));
    CHECK(stroke.painted());

    // Full color inside, nothing outside, and round: the corner of its square is untouched
    CHECK(layerPixel(stack, 1, 100, 100) == (Pixel { 255, 0, 0, 255 }));
    CHECK(layerPixel(stack, 1, 108, 100) == (Pixel { 255, 0, 0, 255 }));
    CHECK(layerPixel(stack, 1, 100, 92) == (Pixel { 255, 0, 0, 255 }));
    CHECK(alphaAt(stack, 1, 111, 100) == 0);
    CHECK(alphaAt(stack, 1, 89, 100) == 0);
    CHECK(alphaAt(stack, 1, 108, 108) == 0);

    // The edge is one pixel of blur, the same on every side
    const u8 edge = alphaAt(stack, 1, 110, 100);
    CHECK(edge == 128);
    CHECK(alphaAt(stack, 1, 90, 100) == edge && alphaAt(stack, 1, 100, 110) == edge && alphaAt(stack, 1, 100, 90) == edge);

    // Only the tile it landed in is new; the layer under it and the stack it started from are as they were
    CHECK(stack.layers[1].tiles[5] != nullptr);
    CHECK(std::count(stack.layers[1].tiles.begin(), stack.layers[1].tiles.end(), nullptr) == 15);
    CHECK(stack.layers[0].tiles == before.layers[0].tiles);
    CHECK(alphaAt(before, 1, 100, 100) == 0);

    // Staying put adds nothing, and so does a stroke that was ended
    CHECK(!stroke.moveTo(stack, Vec2(100.5f, 100.5f)));
    stroke.end();
    CHECK(!stroke.active());
    CHECK(!stroke.moveTo(stack, Vec2(30.0f, 30.0f)));
    CHECK(alphaAt(stack, 1, 30, 30) == 0);
}

TEST_CASE(brush_softness_fades_from_the_middle) {
    LayerStack stack = canvas();
    Stroke stroke;
    stroke.begin(stack, 1, brush(80.0f, 1.0f));
    stroke.moveTo(stack, Vec2(128.0f, 128.0f));

    // Strongest in the middle, weaker every step out, gone at the radius
    u8 last = 255;
    bool fading = alphaAt(stack, 1, 128, 128) >= 250;
    for (u32 x = 132; x <= 164; x += 4) {
        const u8 alpha = alphaAt(stack, 1, x, 128);
        fading = fading && alpha < last;
        last = alpha;
    }
    CHECK(fading);
    CHECK(alphaAt(stack, 1, 148, 128) > 100 && alphaAt(stack, 1, 148, 128) < 155);
    CHECK(alphaAt(stack, 1, 169, 128) == 0);

    // Half soft: full to half the radius, then fading
    LayerStack half = canvas();
    stroke.begin(half, 1, brush(80.0f, 0.5f));
    stroke.moveTo(half, Vec2(128.0f, 128.0f));
    CHECK(alphaAt(half, 1, 146, 128) == 255);
    CHECK(alphaAt(half, 1, 158, 128) > 0 && alphaAt(half, 1, 158, 128) < 255);
}

TEST_CASE(brush_stroke_is_unbroken_and_capped_by_opacity) {
    LayerStack stack = canvas();
    Stroke stroke;
    stroke.begin(stack, 1, brush(16.0f, 0.0f, 0.5f));

    // One fast move across four tiles leaves paint all along the line, at half strength
    stroke.moveTo(stack, Vec2(20.0f, 40.0f));
    CHECK(stroke.moveTo(stack, Vec2(230.0f, 40.0f)));
    bool unbroken = true;
    for (u32 x = 20; x <= 230; ++x) unbroken = unbroken && alphaAt(stack, 1, x, 40) == 128;
    CHECK(unbroken);
    CHECK(alphaAt(stack, 1, 120, 60) == 0);

    // Going back over it, and across it, in the same stroke adds nothing
    stroke.moveTo(stack, Vec2(20.0f, 40.0f));
    stroke.moveTo(stack, Vec2(120.0f, 10.0f));
    stroke.moveTo(stack, Vec2(120.0f, 80.0f));
    bool capped = true;
    for (u32 y = 0; y < 256; ++y) {
        for (u32 x = 0; x < 256; ++x) capped = capped && alphaAt(stack, 1, x, y) <= 128;
    }
    CHECK(capped);
    CHECK(layerPixel(stack, 1, 120, 40) == (Pixel { 255, 0, 0, 128 }));
    CHECK(alphaAt(stack, 1, 120, 70) == 128);

    // A second stroke builds on the first
    stroke.begin(stack, 1, brush(16.0f, 0.0f, 0.5f));
    stroke.moveTo(stack, Vec2(120.0f, 40.0f));
    CHECK(alphaAt(stack, 1, 120, 40) == 192);

    // Wide spacing leaves the dabs apart: one at the start, one a brush width on, nothing between their edges
    LayerStack dotted = canvas();
    Brush dots = brush(10.0f);
    dots.spacing = 1.0f;
    stroke.begin(dotted, 1, dots);
    stroke.moveTo(dotted, Vec2(50.0f, 50.0f));
    stroke.moveTo(dotted, Vec2(75.0f, 50.0f));
    CHECK(alphaAt(dotted, 1, 50, 50) == 255 && alphaAt(dotted, 1, 60, 50) == 255 && alphaAt(dotted, 1, 70, 50) == 255);
    CHECK(alphaAt(dotted, 1, 76, 50) == 0);
    CHECK(alphaAt(dotted, 1, 55, 55) == 0);

    // What a move leaves over carries into the next: 4 then 8 more pixels reach the dab 10 along, and no further
    stroke.begin(dotted, 1, dots);
    stroke.moveTo(dotted, Vec2(50.0f, 150.0f));
    CHECK(!stroke.moveTo(dotted, Vec2(54.0f, 150.0f)));
    CHECK(stroke.moveTo(dotted, Vec2(62.0f, 150.0f)));
    CHECK(alphaAt(dotted, 1, 60, 150) == 255 && alphaAt(dotted, 1, 66, 150) == 0);
}

TEST_CASE(brush_paints_over_what_is_there) {
    // Half-strength red on the opaque white Base mixes, and stays opaque
    LayerStack stack = canvas();
    Stroke stroke;
    stroke.begin(stack, 0, brush(30.0f, 0.0f, 0.5f));
    stroke.moveTo(stack, Vec2(64.0f, 64.0f));
    CHECK(layerPixel(stack, 0, 64, 64) == (Pixel { 255, 127, 127, 255 }));
    CHECK(layerPixel(stack, 0, 100, 64) == (Pixel { 255, 255, 255, 255 }));

    // Base's tiles were one shared tile; only the painted one is its own now
    CHECK(stack.layers[0].tiles[0] != stack.layers[0].tiles[1]);
    CHECK(stack.layers[0].tiles[2] == stack.layers[0].tiles[3]);

    // The brush is clamped when the stroke starts: a wild size and opacity still paint, inside the picture only
    Brush wild = brush(100000.0f, 5.0f, 9.0f);
    stroke.begin(stack, 1, wild);
    CHECK(stroke.moveTo(stack, Vec2(128.0f, 128.0f)));
    CHECK(alphaAt(stack, 1, 128, 128) == 255);

    // Off the picture: a dab half over the edge paints its inside half, one wholly outside paints nothing
    LayerStack edge = canvas();
    stroke.begin(edge, 1, brush(20.0f));
    CHECK(stroke.moveTo(edge, Vec2(0.0f, 128.0f)));
    CHECK(alphaAt(edge, 1, 0, 128) == 255 && alphaAt(edge, 1, 8, 128) == 255 && alphaAt(edge, 1, 11, 128) == 0);
    stroke.begin(edge, 1, brush(20.0f));
    CHECK(!stroke.moveTo(edge, Vec2(-40.0f, 300.0f)));
    CHECK(!stroke.moveTo(edge, Vec2(-40.0f, -300.0f)));
    stroke.begin(edge, 9, brush(20.0f));
    CHECK(!stroke.active());
}

TEST_CASE(eraser_takes_paint_away) {
    LayerStack stack = canvas();
    Brush eraser = brush(20.0f);
    eraser.erase = true;
    Stroke stroke;

    // Full strength clears to transparent, leaving the rest; the combined picture shows through there
    stroke.begin(stack, 0, eraser);
    CHECK(stroke.moveTo(stack, Vec2(64.0f, 64.0f)));
    CHECK(alphaAt(stack, 0, 64, 64) == 0);
    CHECK(layerPixel(stack, 0, 80, 64) == (Pixel { 255, 255, 255, 255 }));
    Pixel combined {};
    compositeLayers(stack, { 64, 64, 1, 1 }, combined.data());
    CHECK(combined[3] == 0);

    // Half strength halves the alpha and keeps the color, once per stroke however often it passes
    eraser.opacity = 0.5f;
    stroke.begin(stack, 0, eraser);
    stroke.moveTo(stack, Vec2(160.0f, 64.0f));
    stroke.moveTo(stack, Vec2(200.0f, 64.0f));
    stroke.moveTo(stack, Vec2(160.0f, 64.0f));
    CHECK(layerPixel(stack, 0, 180, 64) == (Pixel { 255, 255, 255, 127 }));

    // Erasing where the layer is clear does nothing and makes no tiles
    stroke.begin(stack, 1, eraser);
    CHECK(!stroke.moveTo(stack, Vec2(128.0f, 128.0f)));
    CHECK(!stroke.painted());
    CHECK(std::count(stack.layers[1].tiles.begin(), stack.layers[1].tiles.end(), nullptr) == 16);
}

TEST_CASE(strokes_undo_one_at_a_time) {
    Scene scene;
    Texture texture;
    texture.name = "Leaf";
    texture.layers = canvas();
    const TextureHandle handle = scene.textures.add(texture);
    History history;
    Stroke stroke;

    // Two strokes, each its own step
    const auto paint = [&](Vec2 from, Vec2 to) {
        history.begin(scene);
        LayerStack& layers = scene.textures.get(handle).layers;
        stroke.begin(layers, 1, brush(12.0f));
        stroke.moveTo(layers, from);
        stroke.moveTo(layers, to);
        stroke.end();
        history.commit();
    };
    paint(Vec2(20.0f, 20.0f), Vec2(40.0f, 20.0f));
    const std::shared_ptr<Tile> first = scene.textures.get(handle).layers.layers[1].tiles[0];
    paint(Vec2(200.0f, 200.0f), Vec2(220.0f, 200.0f));
    CHECK(alphaAt(scene.textures.get(handle).layers, 1, 30, 20) == 255);
    CHECK(alphaAt(scene.textures.get(handle).layers, 1, 210, 200) == 255);

    // The second stroke left the first one's tile alone, so the steps share it
    CHECK(scene.textures.get(handle).layers.layers[1].tiles[0] == first);

    CHECK(history.undo(scene));
    CHECK(alphaAt(scene.textures.get(handle).layers, 1, 30, 20) == 255);
    CHECK(alphaAt(scene.textures.get(handle).layers, 1, 210, 200) == 0);
    CHECK(history.undo(scene));
    CHECK(alphaAt(scene.textures.get(handle).layers, 1, 30, 20) == 0);
    CHECK(history.redo(scene));
    CHECK(history.redo(scene));
    CHECK(alphaAt(scene.textures.get(handle).layers, 1, 210, 200) == 255);
}

TEST_CASE(old_paint_steps_are_dropped_past_the_budget) {
    Scene scene;
    Texture texture;
    texture.name = "Leaf";
    texture.layers = canvas();
    const TextureHandle handle = scene.textures.add(texture);
    History history;
    Stroke stroke;

    // Every stroke repaints the same tile, so each step but the newest holds one tile of its own
    const auto dot = [&] {
        history.begin(scene);
        LayerStack& layers = scene.textures.get(handle).layers;
        stroke.begin(layers, 1, brush(8.0f, 0.0f, 0.2f));
        stroke.moveTo(layers, Vec2(30.0f, 30.0f));
        stroke.end();
        history.commit();
    };
    for (int i = 0; i < 6; ++i) dot();
    CHECK(history.undoSteps() == 6);

    // Room for three such tiles: the newest step and the three before it stay
    history.setPaintBudget(3 * sizeof(Tile));
    dot();
    CHECK(history.undoSteps() == 4);
    dot();
    CHECK(history.undoSteps() == 4);

    // What's left still undoes, back to the oldest step kept
    int undone = 0;
    while (history.undo(scene)) ++undone;
    CHECK(undone == 4);
    CHECK(alphaAt(scene.textures.get(handle).layers, 1, 30, 30) > 0);

    // No room at all still keeps the newest step
    history.setPaintBudget(0);
    dot();
    dot();
    CHECK(history.undoSteps() == 1);
    CHECK(history.undo(scene));
}

TEST_CASE(lifting_the_brush_breaks_the_line) {
    LayerStack stack = canvas();
    Stroke stroke;
    stroke.begin(stack, 1, brush(10.0f));

    // Two points with the brush lifted between them: a dab at each and nothing in between
    stroke.moveTo(stack, Vec2(40.0f, 40.0f));
    stroke.lift();
    CHECK(stroke.active());
    CHECK(stroke.moveTo(stack, Vec2(200.0f, 40.0f)));
    CHECK(alphaAt(stack, 1, 40, 40) == 255 && alphaAt(stack, 1, 200, 40) == 255);
    CHECK(alphaAt(stack, 1, 120, 40) == 0 && alphaAt(stack, 1, 50, 40) == 0 && alphaAt(stack, 1, 190, 40) == 0);

    // From there the line goes on as usual, and it's still one stroke: crossing the first dab adds nothing to it
    stroke.moveTo(stack, Vec2(200.0f, 100.0f));
    CHECK(alphaAt(stack, 1, 200, 70) == 255);
    stroke.lift();
    stroke.moveTo(stack, Vec2(40.0f, 40.0f));
    CHECK(layerPixel(stack, 1, 40, 40) == (Pixel { 255, 0, 0, 255 }));
    CHECK(alphaAt(stack, 1, 120, 70) == 0);
}
