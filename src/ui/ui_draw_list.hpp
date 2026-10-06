#pragma once

#include <string_view>
#include <vector>
#include "ui/ui_types.hpp"

struct UIVertex {
    Vec2 position;      // pixels
    Vec2 uv;            // font atlas, glyphs only
    Vec2 local;         // offset from the shape's center, in the shape's own axes
    Vec2 halfSize;      // shape half extents
    f32 radius;
    f32 borderWidth;
    f32 blur;           // > 0 draws a soft shadow instead of a solid shape
    f32 mode;           // UIDrawList::MODE_SHAPE, MODE_GLYPH, or MODE_RING_SLICE
    u32 fill;           // packed RGBA
    u32 border;         // packed RGBA
};

// A run of indices drawn with one clip rect and one font texture
struct UIDrawBatch {
    u32 indexOffset = 0;
    u32 indexCount = 0;
    bool clipped = false;
    Rect clip;
    bool hasTexture = false;
    FontId texture = FontId::UI;
};

class UIDrawList {
public:
    static constexpr f32 MODE_SHAPE = 0.0f;
    static constexpr f32 MODE_GLYPH = 1.0f;
    static constexpr f32 MODE_RING_SLICE = 2.0f;

    void clear();

    void rect(const Rect& rect, Color fill);
    void roundedRect(const Rect& rect, f32 radius, Color fill, Color border = {}, f32 borderWidth = 0.0f);
    void shadow(const Rect& rect, f32 radius, f32 blur, Color color);
    void line(Vec2 start, Vec2 end, f32 width, Color color);
    // A wedge of a ring centered on angle (radians, counterclockwise from right); gap is the pixel space between neighbors
    void ringSlice(Vec2 center, f32 innerRadius, f32 outerRadius, f32 angle, f32 halfAngle, f32 gap, Color fill, Color border = {}, f32 borderWidth = 0.0f);
    void text(Vec2 position, std::string_view text, const UIFont& font, Color color);

    // Nested clips intersect with the current one
    void pushClip(const Rect& rect);
    void popClip();

    const std::vector<UIVertex>& getVertices() const { return m_vertices; }
    const std::vector<u32>& getIndices() const { return m_indices; }
    const std::vector<UIDrawBatch>& getBatches() const { return m_batches; }

private:
    void addShape(Vec2 center, Vec2 halfSize, Vec2 axis, f32 radius, f32 borderWidth, f32 blur, Color fill, Color border);
    void addQuad(const UIVertex (&corners)[4], bool needsTexture, FontId texture);

    std::vector<UIVertex> m_vertices;
    std::vector<u32> m_indices;
    std::vector<UIDrawBatch> m_batches;
    std::vector<Rect> m_clipStack;
};
