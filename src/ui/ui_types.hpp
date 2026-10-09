#pragma once

#include <string>

#include <algorithm>
#include <cmath>
#include <string_view>
#include <types>
#include "core/math/vec2.hpp"
#include "core/math/vec3.hpp"
#include "core/font/bitmap_font.hpp"
#include "core/font/font_library.hpp"

// Pixel coordinates, origin top-left, y down
struct Rect {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 width = 0.0f;
    f32 height = 0.0f;

    f32 right() const { return x + width; }
    f32 bottom() const { return y + height; }
    Vec2 center() const { return Vec2(x + width * 0.5f, y + height * 0.5f); }

    bool contains(Vec2 point) const {
        return point.x >= x && point.x < right() && point.y >= y && point.y < bottom();
    }

    bool operator==(const Rect&) const = default;

    static Rect intersect(const Rect& a, const Rect& b) {
        const f32 left = std::max(a.x, b.x);
        const f32 top = std::max(a.y, b.y);
        const f32 right = std::min(a.right(), b.right());
        const f32 bottom = std::min(a.bottom(), b.bottom());
        return { left, top, std::max(0.0f, right - left), std::max(0.0f, bottom - top) };
    }
};

struct Color {
    f32 r = 0.0f;
    f32 g = 0.0f;
    f32 b = 0.0f;
    f32 a = 0.0f;

    // RGBA bytes in memory order, read by the shader as a normalized vec4
    u32 packed() const {
        const auto channel = [](f32 value) { return static_cast<u32>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f)); };
        return channel(r) | (channel(g) << 8) | (channel(b) << 16) | (channel(a) << 24);
    }
};

// Hue, saturation, and value, each 0 to 1 (hue 0 and 1 are both red)
void rgbToHsv(const Vec3& rgb, f32& hue, f32& saturation, f32& value);
Vec3 hsvToRgb(f32 hue, f32 saturation, f32 value);

// "#rrggbb", lowercase
std::string toHex(const Vec3& rgb);
// "#rrggbb", "rrggbb", "#rgb", or "rgb", any case; false (out unchanged) for anything else
bool parseHex(std::string_view text, Vec3& out);

struct UIFont {
    FontId id = FontId::UI;
    f32 glyphWidth = 0.0f;
    f32 glyphHeight = 0.0f;
};

inline UIFont makeUIFont(FontId id, const BitmapFont& font) {
    return { id, static_cast<f32>(font.getGlyphWidth()), static_cast<f32>(font.getGlyphHeight()) };
}

// Size of a block of monospaced text; '\n' starts a new line
inline Vec2 measureText(const UIFont& font, std::string_view text) {
    u32 lines = text.empty() ? 0 : 1;
    u32 longest = 0;
    u32 current = 0;

    for (char character : text) {
        if (character == '\n') {
            ++lines;
            current = 0;
            continue;
        }
        longest = std::max(longest, ++current);
    }

    return Vec2(longest * font.glyphWidth, lines * font.glyphHeight);
}

// text, or as much of it as fits in width followed by "...", so long names end cleanly instead of mid-letter
inline std::string fitText(const UIFont& font, std::string_view text, f32 width) {
    if (measureText(font, text).x <= width) return std::string(text);
    const f32 glyph = font.glyphWidth > 0.0f ? font.glyphWidth : 1.0f;
    const i32 keep = static_cast<i32>(width / glyph) - 3;
    if (keep <= 0) return width >= 3.0f * glyph ? std::string("...") : std::string();
    return std::string(text.substr(0, static_cast<std::size_t>(keep))) + "...";
}
