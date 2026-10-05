#include "ui/ui_draw_list.hpp"
#include "core/font/font_atlas.hpp"

void UIDrawList::clear() {
    m_vertices.clear();
    m_indices.clear();
    m_batches.clear();
    m_clipStack.clear();
}

void UIDrawList::rect(const Rect& rect, Color fill) {
    roundedRect(rect, 0.0f, fill);
}

void UIDrawList::roundedRect(const Rect& rect, f32 radius, Color fill, Color border, f32 borderWidth) {
    addShape(rect.center(), Vec2(rect.width * 0.5f, rect.height * 0.5f), Vec2(1.0f, 0.0f), radius, borderWidth, 0.0f, fill, border);
}

void UIDrawList::shadow(const Rect& rect, f32 radius, f32 blur, Color color) {
    addShape(rect.center(), Vec2(rect.width * 0.5f, rect.height * 0.5f), Vec2(1.0f, 0.0f), radius, 0.0f, blur, color, {});
}

void UIDrawList::line(Vec2 start, Vec2 end, f32 width, Color color) {
    const Vec2 delta = end - start;
    const f32 length = delta.length();
    if (length <= 0.0f) return;

    addShape((start + end) * 0.5f, Vec2(length * 0.5f, width * 0.5f), delta / length, 0.0f, 0.0f, 0.0f, color, {});
}

void UIDrawList::text(Vec2 position, std::string_view text, const UIFont& font, Color color) {
    // Whole pixels keep glyph texels aligned with screen pixels
    const f32 startX = std::round(position.x);
    f32 x = startX;
    f32 y = std::round(position.y);

    const u32 fill = color.packed();

    for (char character : text) {
        if (character == '\n') {
            x = startX;
            y += font.glyphHeight;
            continue;
        }

        if (character != ' ') {
            const GlyphUV uv = FontAtlas::glyphUV(character);
            const f32 right = x + font.glyphWidth;
            const f32 bottom = y + font.glyphHeight;

            const UIVertex corners[4] = {
                { Vec2(x, y),         Vec2(uv.u0, uv.v0), Vec2(), Vec2(), 0, 0, 0, MODE_GLYPH, fill, 0 },
                { Vec2(right, y),     Vec2(uv.u1, uv.v0), Vec2(), Vec2(), 0, 0, 0, MODE_GLYPH, fill, 0 },
                { Vec2(right, bottom), Vec2(uv.u1, uv.v1), Vec2(), Vec2(), 0, 0, 0, MODE_GLYPH, fill, 0 },
                { Vec2(x, bottom),    Vec2(uv.u0, uv.v1), Vec2(), Vec2(), 0, 0, 0, MODE_GLYPH, fill, 0 },
            };
            addQuad(corners, true, font.id);
        }

        x += font.glyphWidth;
    }
}

void UIDrawList::pushClip(const Rect& rect) {
    m_clipStack.push_back(m_clipStack.empty() ? rect : Rect::intersect(m_clipStack.back(), rect));
}

void UIDrawList::popClip() {
    if (!m_clipStack.empty()) m_clipStack.pop_back();
}

void UIDrawList::addShape(Vec2 center, Vec2 halfSize, Vec2 axis, f32 radius, f32 borderWidth, f32 blur, Color fill, Color border) {
    // Pad past the shape's edge so anti-aliasing and blur have pixels to fade into
    const f32 pad = 1.0f + blur;
    const Vec2 extent(halfSize.x + pad, halfSize.y + pad);
    const Vec2 perpendicular(-axis.y, axis.x);

    const u32 packedFill = fill.packed();
    const u32 packedBorder = border.packed();

    const Vec2 locals[4] = {
        Vec2(-extent.x, -extent.y), Vec2(extent.x, -extent.y),
        Vec2(extent.x, extent.y),   Vec2(-extent.x, extent.y)
    };

    UIVertex corners[4];
    for (u32 i = 0; i < 4; ++i) {
        corners[i] = {
            center + axis * locals[i].x + perpendicular * locals[i].y,
            Vec2(), locals[i], halfSize,
            radius, borderWidth, blur, MODE_SHAPE,
            packedFill, packedBorder
        };
    }

    addQuad(corners, false, FontId::UI);
}

void UIDrawList::addQuad(const UIVertex (&corners)[4], bool needsTexture, FontId texture) {
    const bool clipped = !m_clipStack.empty();
    const Rect clip = clipped ? m_clipStack.back() : Rect();

    // Start a new batch when the clip changes or a different font texture is needed
    bool newBatch = m_batches.empty();
    if (!newBatch) {
        const UIDrawBatch& last = m_batches.back();
        newBatch = last.clipped != clipped || (clipped && !(last.clip == clip))
                || (needsTexture && last.hasTexture && last.texture != texture);
    }

    if (newBatch) {
        UIDrawBatch batch;
        batch.indexOffset = static_cast<u32>(m_indices.size());
        batch.clipped = clipped;
        batch.clip = clip;
        m_batches.push_back(batch);
    }

    UIDrawBatch& batch = m_batches.back();
    if (needsTexture && !batch.hasTexture) {
        batch.hasTexture = true;
        batch.texture = texture;
    }

    const u32 base = static_cast<u32>(m_vertices.size());
    m_vertices.insert(m_vertices.end(), std::begin(corners), std::end(corners));
    m_indices.insert(m_indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
    batch.indexCount += 6;
}
