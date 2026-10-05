#pragma once

#include "ui/ui_types.hpp"

// One place for every size and color the widgets use
namespace UIStyle {
    constexpr f32 PADDING = 10.0f;
    constexpr f32 ROW_HEIGHT = 24.0f;
    constexpr f32 ITEM_SPACING = 4.0f;
    constexpr f32 SECTION_SPACING = 10.0f;
    constexpr f32 CORNER_RADIUS = 4.0f;
    constexpr f32 LABEL_FRACTION = 0.38f;
    constexpr f32 TEXT_PADDING = 8.0f;
    constexpr f32 INDENT = 14.0f;

    constexpr f32 CHECKBOX_SIZE = 14.0f;
    constexpr f32 SELECTED_BAR_WIDTH = 2.0f;
    constexpr f32 COMPONENT_GAP = 4.0f;

    constexpr f32 PANEL_RADIUS = 8.0f;
    constexpr f32 PANEL_SHADOW_BLUR = 14.0f;
    constexpr f32 PANEL_SHADOW_OFFSET = 6.0f;
    constexpr f32 PANEL_HEADER_HEIGHT = 32.0f;
    constexpr f32 PANEL_RESIZE_GRIP = 5.0f;
    constexpr f32 PANEL_MIN_WIDTH = 240.0f;
    constexpr f32 PANEL_MIN_HEIGHT = 160.0f;

    const Color TEXT { 0.86f, 0.86f, 0.88f, 1.0f };
    const Color TEXT_DIM { 0.55f, 0.55f, 0.60f, 1.0f };
    const Color TEXT_ON_ACCENT { 0.10f, 0.08f, 0.05f, 1.0f };

    const Color FRAME { 0.18f, 0.18f, 0.21f, 1.0f };
    const Color FRAME_HOVER { 0.23f, 0.23f, 0.27f, 1.0f };
    const Color FRAME_ACTIVE { 0.27f, 0.27f, 0.32f, 1.0f };
    const Color FRAME_BORDER { 0.30f, 0.30f, 0.35f, 1.0f };

    // Same warm amber as viewport selection
    const Color ACCENT { 1.0f, 0.76f, 0.30f, 1.0f };
    const Color ACCENT_FILL { 0.80f, 0.58f, 0.22f, 0.55f };
    const Color ACCENT_SOFT { 1.0f, 0.76f, 0.30f, 0.18f };

    const Color ROW_HOVER { 1.0f, 1.0f, 1.0f, 0.05f };
    const Color SEPARATOR { 0.30f, 0.30f, 0.34f, 1.0f };

    const Color PANEL_BACKGROUND { 0.14f, 0.14f, 0.16f, 0.96f };
    const Color PANEL_BORDER { 0.30f, 0.30f, 0.35f, 1.0f };
    const Color PANEL_SHADOW { 0.0f, 0.0f, 0.0f, 0.5f };
    const Color PANEL_HEADER { 0.17f, 0.17f, 0.20f, 1.0f };
    const Color PANEL_HEADER_HOVER { 0.20f, 0.20f, 0.24f, 1.0f };

    // Same hues as the grid axes and status bar
    const Color AXIS_X { 0.95f, 0.35f, 0.40f, 1.0f };
    const Color AXIS_Y { 0.50f, 0.85f, 0.35f, 1.0f };
    const Color AXIS_Z { 0.40f, 0.60f, 1.00f, 1.0f };
}
