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

    // Colors follow the Dracula theme: purple marks selection and highlights, green headings and whatever is being typed into
    const Color TEXT { 0.97f, 0.97f, 0.95f, 1.0f };                // foreground #f8f8f2
    const Color TEXT_DIM { 0.46f, 0.52f, 0.70f, 1.0f };            // comment #6272a4, a little brighter for labels
    const Color TEXT_ON_ACCENT { 0.10f, 0.10f, 0.13f, 1.0f };

    const Color FRAME { 0.20f, 0.21f, 0.27f, 1.0f };
    const Color FRAME_HOVER { 0.24f, 0.25f, 0.32f, 1.0f };
    const Color FRAME_ACTIVE { 0.27f, 0.28f, 0.35f, 1.0f };        // current line #44475a
    const Color FRAME_BORDER { 0.30f, 0.31f, 0.40f, 1.0f };

    // Purple #bd93f9, same as viewport selection
    const Color ACCENT { 0.74f, 0.58f, 0.98f, 1.0f };
    const Color ACCENT_FILL { 0.74f, 0.58f, 0.98f, 0.45f };
    const Color ACCENT_SOFT { 0.74f, 0.58f, 0.98f, 0.18f };

    // Green #50fa7b: headings, and the field or console line being typed into
    const Color ACCENT_GREEN { 0.31f, 0.98f, 0.48f, 1.0f };

    // Red #ff5555: errors
    const Color ERROR { 1.0f, 0.33f, 0.33f, 1.0f };

    const Color ROW_HOVER { 1.0f, 1.0f, 1.0f, 0.05f };
    const Color SEPARATOR { 0.27f, 0.28f, 0.35f, 1.0f };

    const Color PANEL_BACKGROUND { 0.16f, 0.16f, 0.21f, 0.97f };   // background #282a36
    const Color PANEL_BORDER { 0.27f, 0.28f, 0.35f, 1.0f };
    const Color PANEL_SHADOW { 0.0f, 0.0f, 0.0f, 0.5f };
    const Color PANEL_HEADER { 0.13f, 0.13f, 0.17f, 1.0f };        // darker background #21222c
    const Color PANEL_HEADER_HOVER { 0.17f, 0.17f, 0.22f, 1.0f };
    const Color LIST_BACKGROUND { 0.13f, 0.13f, 0.17f, 1.0f };
    const Color SCROLLBAR_TRACK { 1.0f, 1.0f, 1.0f, 0.04f };
    const Color SCROLLBAR_THUMB { 0.38f, 0.45f, 0.64f, 0.55f };
    const Color SCROLLBAR_THUMB_HOVER { 0.38f, 0.45f, 0.64f, 0.85f };

    // Faint viewport guides: light marker drop lines and the scale/rotate/bevel lines to the mouse
    const Color GUIDE_LINE { 0.97f, 0.97f, 0.95f, 0.30f };
    const Color GUIDE_DOT { 0.97f, 0.97f, 0.95f, 0.55f };
    // Dark edge under tool guides so they read on light faces too
    const Color GUIDE_HALO { 0.10f, 0.10f, 0.13f, 0.45f };

    // Red, green, and cyan from the palette; same hues as the grid axes and status bar
    const Color AXIS_X { 1.0f, 0.33f, 0.33f, 1.0f };
    const Color AXIS_Y { 0.31f, 0.98f, 0.48f, 1.0f };
    const Color AXIS_Z { 0.55f, 0.91f, 0.99f, 1.0f };
}
