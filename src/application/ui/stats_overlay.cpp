#include "application/ui/stats_overlay.hpp"
#include "application/workspace.hpp"
#include "application/app_context.hpp"
#include "ui/ui_style.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace {
    constexpr f32 MARGIN = 12.0f;
    constexpr f32 PADDING = 8.0f;
    constexpr f32 LINE_GAP = 2.0f;
    constexpr u32 LABEL_COLUMNS = 13;

    std::string milliseconds(f32 value) {
        if (value < 0.0f) return "-";
        char text[32];
        std::snprintf(text, sizeof(text), "%.2f ms", value);
        return text;
    }

    // 1234567 as "1,234,567"
    std::string grouped(u64 value) {
        std::string digits = std::to_string(value);
        for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3) digits.insert(static_cast<std::size_t>(i), ",");
        return digits;
    }

    std::string bytes(u64 value) {
        char text[32];
        if (value >= 1024 * 1024) std::snprintf(text, sizeof(text), "%.1f MB", value / (1024.0 * 1024.0));
        else if (value >= 1024) std::snprintf(text, sizeof(text), "%.1f KB", value / 1024.0);
        else std::snprintf(text, sizeof(text), "%llu B", static_cast<unsigned long long>(value));
        return text;
    }
}

void drawStatsOverlay(const AppContext& ctx, UIDrawList& ui) {
    if (!ctx.viewport.showStats) return;

    const RenderStats render = ctx.renderer->getStats();
    const FrameStats& frame = ctx.frameStats;

    u64 vertices = 0, faces = 0;
    for (ObjectHandle handle : ctx.scene.objects.handles()) {
        const MeshData& mesh = ctx.scene.objects.get(handle).meshData;
        vertices += mesh.getVertexHandles().size();
        faces += mesh.getFaceHandles().size();
    }

    const std::vector<std::pair<std::string, std::string>> lines = {
        { "CPU input", milliseconds(frame.inputMilliseconds) },
        { "CPU render", milliseconds(frame.renderMilliseconds) },
        { "GPU", milliseconds(render.gpuMilliseconds) },
        { "Last pick", milliseconds(frame.lastPickMilliseconds) },
        { "Draw calls", grouped(render.drawCalls) },
        { "Triangles", grouped(render.triangles) },
        { "Lines", grouped(render.lines) },
        { "Points", grouped(render.points) },
        { "Uniforms", grouped(render.uniformUploads) },
        { "Uploads", std::to_string(render.meshUploads) + (render.meshUploads == 0 ? "" : ", " + bytes(render.meshUploadBytes)) },
        { "Patches", std::to_string(render.meshPatches) + (render.meshPatches == 0 ? "" : ", " + bytes(render.meshPatchBytes)) },
        { "Objects", grouped(ctx.scene.objects.count()) },
        { "Faces", grouped(faces) },
        { "Vertices", grouped(vertices) },
    };

    const UIFont font = makeUIFont(FontId::UI, ctx.fonts.get(FontId::UI));
    std::size_t widest = 0;
    for (const auto& [label, value] : lines) widest = std::max(widest, value.size());

    const f32 lineHeight = font.glyphHeight + LINE_GAP;
    const Rect view = sceneView(ctx);
    const Rect box { view.x + MARGIN, view.y + MARGIN, (LABEL_COLUMNS + widest) * font.glyphWidth + PADDING * 2.0f, lines.size() * lineHeight - LINE_GAP + PADDING * 2.0f };
    ui.roundedRect(box, UIStyle::CORNER_RADIUS, UIStyle::PANEL_BACKGROUND, UIStyle::PANEL_BORDER, 1.0f);

    f32 y = box.y + PADDING;
    for (const auto& [label, value] : lines) {
        ui.text(Vec2(box.x + PADDING, y), label, font, UIStyle::TEXT_DIM);
        ui.text(Vec2(box.x + PADDING + LABEL_COLUMNS * font.glyphWidth, y), value, font, UIStyle::TEXT);
        y += lineHeight;
    }
}
