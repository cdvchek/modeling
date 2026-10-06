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

    constexpr f32 SCROLL_STEP = 48.0f;            // pixels per wheel notch
    constexpr f32 SCROLLBAR_WIDTH = 4.0f;
    constexpr f32 SCROLLBAR_INSET = 5.0f;         // clear of the resize grip, still inside the padding
    constexpr f32 SCROLLBAR_MIN_THUMB = 24.0f;
    constexpr f32 CHILD_PADDING = 4.0f;
    constexpr f32 TAB_PADDING = 14.0f;
    constexpr f32 TAB_INSET = 5.0f;              // gap above and beside the tabs inside the header
    constexpr f32 TAB_UNDERLINE = 2.0f;
    constexpr f32 POPUP_PADDING = 4.0f;

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
    const Color LIST_BACKGROUND { 0.11f, 0.11f, 0.13f, 1.0f };
    const Color SCROLLBAR_TRACK { 1.0f, 1.0f, 1.0f, 0.04f };
    const Color SCROLLBAR_THUMB { 1.0f, 1.0f, 1.0f, 0.22f };
    const Color SCROLLBAR_THUMB_HOVER { 1.0f, 1.0f, 1.0f, 0.35f };

    // Faint viewport guides: light marker drop lines and the scale/rotate/bevel lines to the mouse
    const Color GUIDE_LINE { 0.70f, 0.70f, 0.75f, 0.30f };
    const Color GUIDE_DOT { 0.70f, 0.70f, 0.75f, 0.55f };
    // Dark edge under tool guides so they read on light faces too
    const Color GUIDE_HALO { 0.05f, 0.05f, 0.07f, 0.45f };

    // Same hues as the grid axes and status bar
    const Color AXIS_X { 0.95f, 0.35f, 0.40f, 1.0f };
    const Color AXIS_Y { 0.50f, 0.85f, 0.35f, 1.0f };
    const Color AXIS_Z { 0.40f, 0.60f, 1.00f, 1.0f };
}
