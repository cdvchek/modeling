#include "test.hpp"
#include "ui/ui_draw_list.hpp"

namespace {
    const UIFont FONT { FontId::UI, 8.0f, 16.0f };
    const Color WHITE { 1.0f, 1.0f, 1.0f, 1.0f };
}

TEST_CASE(ui_shapes_share_one_batch) {
    UIDrawList list;
    list.rect({ 0, 0, 10, 10 }, WHITE);
    list.roundedRect({ 20, 0, 10, 10 }, 4.0f, WHITE, WHITE, 1.0f);
    list.line(Vec2(0, 0), Vec2(10, 10), 2.0f, WHITE);

    CHECK(list.getVertices().size() == 12);
    CHECK(list.getIndices().size() == 18);
    CHECK(list.getBatches().size() == 1);
    CHECK(list.getBatches()[0].indexCount == 18);
    CHECK(!list.getBatches()[0].clipped);
}

TEST_CASE(ui_text_skips_spaces_and_wraps_lines) {
    UIDrawList list;
    list.text(Vec2(10.4f, 20.6f), "ab c\nd", FONT, WHITE);

    // a, b, c, d: four quads; the space advances without drawing
    CHECK(list.getVertices().size() == 16);
    CHECK(list.getBatches().size() == 1);
    CHECK(list.getBatches()[0].hasTexture);

    // Snapped to whole pixels; 'c' is three cells over, 'd' starts the second line
    CHECK(list.getVertices()[0].position.x == 10.0f);
    CHECK(list.getVertices()[0].position.y == 21.0f);
    CHECK(list.getVertices()[8].position.x == 10.0f + 3 * 8.0f);
    CHECK(list.getVertices()[12].position.y == 21.0f + 16.0f);
}

TEST_CASE(ui_text_and_shapes_mix_in_one_batch) {
    UIDrawList list;
    list.rect({ 0, 0, 100, 20 }, WHITE);
    list.text(Vec2(4, 2), "hi", FONT, WHITE);
    list.rect({ 0, 20, 100, 20 }, WHITE);

    CHECK(list.getBatches().size() == 1);
}

TEST_CASE(ui_font_change_starts_new_batch) {
    UIDrawList list;
    list.text(Vec2(0, 0), "a", FONT, WHITE);
    list.text(Vec2(0, 20), "b", UIFont { FontId::Console, 16.0f, 24.0f }, WHITE);

    CHECK(list.getBatches().size() == 2);
    CHECK(list.getBatches()[1].texture == FontId::Console);
    CHECK(list.getBatches()[1].indexOffset == 6);
}

TEST_CASE(ui_clips_nest_and_split_batches) {
    UIDrawList list;
    list.rect({ 0, 0, 10, 10 }, WHITE);

    list.pushClip({ 0, 0, 100, 100 });
    list.pushClip({ 50, 50, 100, 100 });
    list.rect({ 0, 0, 10, 10 }, WHITE);
    list.popClip();
    list.rect({ 0, 0, 10, 10 }, WHITE);
    list.popClip();

    const auto& batches = list.getBatches();
    CHECK(batches.size() == 3);
    CHECK(batches[1].clipped);
    CHECK(batches[1].clip == (Rect { 50, 50, 50, 50 }));
    CHECK(batches[2].clip == (Rect { 0, 0, 100, 100 }));
}

TEST_CASE(ui_measure_text) {
    CHECK(measureText(FONT, "").x == 0.0f);
    CHECK(measureText(FONT, "abc").x == 24.0f);
    CHECK(measureText(FONT, "abc").y == 16.0f);
    CHECK(measureText(FONT, "a\nlonger").x == 48.0f);
    CHECK(measureText(FONT, "a\nlonger").y == 32.0f);
}

TEST_CASE(ui_color_packs_rgba_bytes) {
    CHECK((Color { 1.0f, 0.0f, 0.0f, 1.0f }).packed() == 0xFF0000FFu);
    CHECK((Color { 0.0f, 0.0f, 1.0f, 0.5f }).packed() == 0x80FF0000u);
}
