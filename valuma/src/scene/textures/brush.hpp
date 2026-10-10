#pragma once

#include <array>
#include <memory>
#include <vector>
#include <types>
#include "core/math/vec2.hpp"
#include "core/math/vec3.hpp"
#include "scene/textures/layers.hpp"
#include "scene/textures/paint_mask.hpp"

// Painting into a layer: a brush stamps round dabs along the path of a stroke

inline constexpr f32 MIN_BRUSH_SIZE = 1.0f;
inline constexpr f32 MAX_BRUSH_SIZE = 512.0f;
inline constexpr f32 MIN_BRUSH_SPACING = 0.02f;
inline constexpr f32 MAX_BRUSH_SPACING = 1.0f;

struct Brush {
    Vec3 color { 0.0f, 0.0f, 0.0f };    // sRGB, as the color picker shows it
    f32 size = 24.0f;                   // across, in texture pixels
    f32 softness = 0.5f;                // 0: a crisp edge; 1: fading from the middle out
    f32 opacity = 1.0f;                 // the most one stroke lays down, however often it crosses itself
    f32 spacing = 0.1f;                 // between dabs, as a part of the size
    bool erase = false;                 // takes the layer's paint away instead of adding color

    // Every value inside its range (after loading, or typing)
    void clamp();
};

// One press-drag-release on one layer; crossing a spot again in the same stroke never adds more than the brush's opacity
class Stroke {
public:
    // Starts a stroke on a layer; nothing is painted until the first moveTo
    void begin(const LayerStack& stack, u32 layer, const Brush& brush);

    // Carries the stroke to a point in texture pixels (0, 0 is the top left corner), stamping dabs along the way; true if any pixel changed
    bool moveTo(LayerStack& stack, Vec2 point);

    void end();

    // Lifts the brush without ending the stroke: the next moveTo starts with a dab of its own instead of a line from the last point
    void lift();

    // Limits the dabs that follow to a mask of the texture's size (null: no limit); set after begin, and changeable between moves
    void setMask(std::shared_ptr<const PaintMask> mask) { m_mask = std::move(mask); }

    bool active() const { return m_active; }
    // Any pixel changed since begin
    bool painted() const { return m_painted; }

private:
    using Coverage = std::array<u8, TILE_SIZE * TILE_SIZE>;

    bool dab(LayerStack& stack, Vec2 center);

    Brush m_brush;
    u32 m_layer = 0;
    bool m_active = false;
    bool m_started = false;
    bool m_painted = false;
    Vec2 m_last;
    // How far the stroke has come since its last dab
    f32 m_sinceDab = 0.0f;
    // The layer's tiles when the stroke began, and per tile how much brush each pixel has had (none until touched)
    std::vector<std::shared_ptr<Tile>> m_before;
    std::vector<std::unique_ptr<Coverage>> m_coverage;
    std::shared_ptr<const PaintMask> m_mask;
};
