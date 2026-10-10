#include "scene/textures/brush.hpp"

#include <algorithm>
#include <cmath>

namespace {
    // Dabs are never closer than this many pixels, however small the brush or its spacing
    constexpr f32 MIN_STEP = 0.25f;

    u8 channel(f32 value) {
        return static_cast<u8>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
    }
}

void Brush::clamp() {
    const auto unit = [](f32& value, f32 fallback) { value = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : fallback; };
    unit(color.x, 0.0f);
    unit(color.y, 0.0f);
    unit(color.z, 0.0f);
    unit(softness, 0.5f);
    unit(opacity, 1.0f);
    size = std::isfinite(size) ? std::clamp(size, MIN_BRUSH_SIZE, MAX_BRUSH_SIZE) : 24.0f;
    spacing = std::isfinite(spacing) ? std::clamp(spacing, MIN_BRUSH_SPACING, MAX_BRUSH_SPACING) : 0.1f;
}

void Stroke::begin(const LayerStack& stack, u32 layer, const Brush& brush) {
    end();
    if (layer >= stack.layers.size()) return;

    m_brush = brush;
    m_brush.clamp();
    m_layer = layer;
    m_before = stack.layers[layer].tiles;
    m_coverage.resize(m_before.size());
    m_active = true;
}

void Stroke::end() {
    m_active = m_started = m_painted = false;
    m_sinceDab = 0.0f;
    m_before.clear();
    m_coverage.clear();
    m_mask.reset();
}

void Stroke::lift() {
    m_started = false;
    m_sinceDab = 0.0f;
}

bool Stroke::moveTo(LayerStack& stack, Vec2 point) {
    if (!m_active || m_layer >= stack.layers.size() || stack.layers[m_layer].tiles.size() != m_before.size()) return false;

    // The first point gets a dab of its own
    bool changed = false;
    if (!m_started) {
        m_started = true;
        m_last = point;
        changed = dab(stack, point);
        m_painted = m_painted || changed;
        return changed;
    }

    // Then one every step along the way, counting what the last move left over
    const f32 step = std::max(m_brush.size * m_brush.spacing, MIN_STEP);
    const Vec2 delta = point - m_last;
    const f32 length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    f32 at = step - m_sinceDab;
    for (; at <= length; at += step) changed = dab(stack, m_last + delta * (at / length)) || changed;
    m_sinceDab = length - (at - step);
    m_last = point;

    m_painted = m_painted || changed;
    return changed;
}

bool Stroke::dab(LayerStack& stack, Vec2 center) {
    if (m_mask && (m_mask->width != stack.width || m_mask->height != stack.height)) return false;

    // Full inside inner, nothing past outer: a crisp edge is one pixel of blur, a soft one reaches in toward the middle
    const f32 radius = m_brush.size * 0.5f;
    const f32 inner = std::min(radius * (1.0f - m_brush.softness), radius - 0.5f);
    const f32 outer = std::max(radius, inner + 1.0f);
    const f32 innerSquared = inner > 0.0f ? inner * inner : -1.0f;

    // The pixels whose middles the dab can reach, kept to the picture
    const f32 left = std::floor(center.x - outer), top = std::floor(center.y - outer);
    const f32 right = std::ceil(center.x + outer), bottom = std::ceil(center.y + outer);
    if (right <= 0.0f || bottom <= 0.0f || left >= static_cast<f32>(stack.width) || top >= static_cast<f32>(stack.height)) return false;
    const u32 x0 = static_cast<u32>(std::max(left, 0.0f)), y0 = static_cast<u32>(std::max(top, 0.0f));
    const u32 x1 = static_cast<u32>(std::min(right, static_cast<f32>(stack.width))), y1 = static_cast<u32>(std::min(bottom, static_cast<f32>(stack.height)));

    const u32 opacity = channel(m_brush.opacity);
    const u8 color[4] = { channel(m_brush.color.x), channel(m_brush.color.y), channel(m_brush.color.z), 255 };
    static const Tile CLEAR;

    bool changed = false;
    for (u32 tileY = y0 / TILE_SIZE; tileY * TILE_SIZE < y1; ++tileY) {
        for (u32 tileX = x0 / TILE_SIZE; tileX * TILE_SIZE < x1; ++tileX) {
            const std::size_t index = std::size_t(tileY) * stack.tilesAcross() + tileX;
            // There's nothing to erase where the layer was clear
            if (m_brush.erase && !m_before[index]) continue;

            const u32 tileLeft = tileX * TILE_SIZE, tileTop = tileY * TILE_SIZE;
            const u32 fromX = std::max(x0, tileLeft), toX = std::min(x1, tileLeft + TILE_SIZE);
            const u32 fromY = std::max(y0, tileTop), toY = std::min(y1, tileTop + TILE_SIZE);
            const Tile& before = m_before[index] ? *m_before[index] : CLEAR;
            Tile* tile = nullptr;

            for (u32 y = fromY; y < toY; ++y) {
                const f32 dy = (static_cast<f32>(y) + 0.5f) - center.y;
                for (u32 x = fromX; x < toX; ++x) {
                    const f32 dx = (static_cast<f32>(x) + 0.5f) - center.x;
                    const f32 squared = dx * dx + dy * dy;
                    if (squared >= outer * outer) continue;

                    f32 amount = 1.0f;
                    if (squared > innerSquared) {
                        const f32 t = std::clamp((std::sqrt(squared) - inner) / (outer - inner), 0.0f, 1.0f);
                        amount = 1.0f - t * t * (3.0f - 2.0f * t);
                    }
                    u8 reach = static_cast<u8>(amount * 255.0f + 0.5f);
                    if (m_mask) reach = static_cast<u8>((reach * m_mask->at(x, y) + 127) / 255);

                    // Only a pixel this dab reaches further than any before it changes
                    if (!m_coverage[index]) m_coverage[index] = std::make_unique<Coverage>(Coverage {});
                    const std::size_t pixel = std::size_t(y - tileTop) * TILE_SIZE + (x - tileLeft);
                    u8& reached = (*m_coverage[index])[pixel];
                    if (reach <= reached) continue;
                    reached = reach;

                    // The first change to a tile in a frame takes a copy, so the stroke's start stays as it was
                    if (!tile) tile = &editTile(stack, m_layer, tileX, tileY);
                    const u8* was = before.pixels.data() + pixel * 4;
                    u8* now = tile->pixels.data() + pixel * 4;
                    const u32 strength = (reach * opacity + 127) / 255;
                    std::copy(was, was + 4, now);
                    if (m_brush.erase) now[3] = static_cast<u8>((was[3] * (255 - strength) + 127) / 255);
                    else blendPixel(now, color, strength);
                }
            }
            changed = changed || tile != nullptr;
        }
    }
    return changed;
}
