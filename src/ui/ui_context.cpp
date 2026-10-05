#include "ui/ui_context.hpp"
#include "ui/ui_style.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace {
    constexpr u32 FNV_OFFSET = 2166136261u;
    constexpr u32 FNV_PRIME = 16777619u;

    u32 hashBytes(const void* data, std::size_t size, u32 seed) {
        u32 hash = seed;
        const unsigned char* bytes = static_cast<const unsigned char*>(data);
        for (std::size_t i = 0; i < size; ++i) {
            hash ^= bytes[i];
            hash *= FNV_PRIME;
        }
        return hash;
    }
}

void UIContext::beginFrame(const UIInput& input, bool interactive) {
    m_input = input;
    m_interactive = interactive;

    if (!m_interactive) m_activeId = 0;

    // Topmost region from last frame under the mouse
    m_hoveredRegion = -1;
    for (i32 i = static_cast<i32>(m_previousRegions.size()) - 1; i >= 0; --i) {
        if (m_previousRegions[i].contains(m_input.mouse)) {
            m_hoveredRegion = i;
            break;
        }
    }

    // A press that starts outside the UI belongs to the viewport until every button is released
    if (m_input.anyPressed() && m_hoveredRegion < 0 && m_activeId == 0) m_viewportOwnsMouse = true;
    if (!m_input.anyDown() && !m_input.anyPressed()) m_viewportOwnsMouse = false;
    if (m_viewportOwnsMouse) m_hoveredRegion = -1;

    m_wantsMouse = m_interactive && (m_hoveredRegion >= 0 || m_activeId != 0);
}

void UIContext::beginDraw() {
    m_cursor = UICursor::Arrow;
    m_hotId = 0;
    m_activeSeen = false;
    m_regions.clear();
    m_idStack.clear();
    m_currentRegion = -1;
    m_last = {};
}

void UIContext::endDraw() {
    m_previousRegions = m_regions;

    // A widget that stopped being drawn mid-drag (e.g. its light was deleted) can't hold the mouse forever
    if (!m_activeSeen || !m_input.down[UIInput::LEFT]) m_activeId = 0;

    // A second draw in the same frame (e.g. while resizing) must not see the clicks again
    for (u32 i = 0; i < UIInput::BUTTON_COUNT; ++i) {
        m_input.pressed[i] = false;
        m_input.released[i] = false;
    }
    m_input.mouseDelta = Vec2();
    m_input.scroll = 0;
}

void UIContext::beginRegion(const Rect& rect) {
    m_currentRegion = static_cast<i32>(m_regions.size());
    m_regions.push_back(rect);

    m_layout.area = { rect.x + UIStyle::PADDING, rect.y + UIStyle::PADDING, rect.width - UIStyle::PADDING * 2.0f, rect.height - UIStyle::PADDING * 2.0f };
    m_layout.cursorY = m_layout.area.y;
    m_layout.indent = 0.0f;

    pushId(static_cast<u32>(m_currentRegion));
    if (m_drawList) m_drawList->pushClip(rect);
}

void UIContext::endRegion() {
    if (m_drawList) m_drawList->popClip();
    popId();
    m_currentRegion = -1;
}

namespace {
    constexpr u32 EDGE_LEFT = 1;
    constexpr u32 EDGE_RIGHT = 2;
    constexpr u32 EDGE_TOP = 4;
    constexpr u32 EDGE_BOTTOM = 8;

    // Shrinks the panel to fit if needed, then moves it inside
    Rect clampInside(Rect rect, const Rect& bounds) {
        rect.width = std::min(rect.width, bounds.width);
        rect.height = std::min(rect.height, bounds.height);
        rect.x = std::max(bounds.x, std::min(rect.x, bounds.right() - rect.width));
        rect.y = std::max(bounds.y, std::min(rect.y, bounds.bottom() - rect.height));
        return rect;
    }

    // Which edges the mouse is on; the grip reaches a little past the panel as well as inside it
    u32 edgesUnderMouse(const Rect& rect, Vec2 mouse) {
        const f32 grip = UIStyle::PANEL_RESIZE_GRIP;
        const Rect grab { rect.x - grip, rect.y - grip, rect.width + grip * 2.0f, rect.height + grip * 2.0f };
        if (!grab.contains(mouse)) return 0;

        u32 edges = 0;
        if (mouse.x < rect.x + grip) edges |= EDGE_LEFT;
        if (mouse.x >= rect.right() - grip) edges |= EDGE_RIGHT;
        if (mouse.y < rect.y + grip) edges |= EDGE_TOP;
        if (mouse.y >= rect.bottom() - grip) edges |= EDGE_BOTTOM;
        return edges;
    }

    UICursor resizeCursor(u32 edges) {
        const bool horizontal = edges & (EDGE_LEFT | EDGE_RIGHT);
        const bool vertical = edges & (EDGE_TOP | EDGE_BOTTOM);
        if (horizontal && vertical) {
            const bool down = (edges & EDGE_LEFT && edges & EDGE_TOP) || (edges & EDGE_RIGHT && edges & EDGE_BOTTOM);
            return down ? UICursor::ResizeDiagonalDown : UICursor::ResizeDiagonalUp;
        }
        if (horizontal) return UICursor::ResizeHorizontal;
        if (vertical) return UICursor::ResizeVertical;
        return UICursor::Arrow;
    }

    // Moves the dragged edges by delta; the opposite edges stay put, and size and bounds limits hold
    Rect resizeRect(const Rect& start, u32 edges, Vec2 delta, const Rect& bounds) {
        f32 left = start.x;
        f32 right = start.right();
        f32 top = start.y;
        f32 bottom = start.bottom();

        if (edges & EDGE_LEFT) left = std::clamp(start.x + delta.x, bounds.x, right - UIStyle::PANEL_MIN_WIDTH);
        if (edges & EDGE_RIGHT) right = std::clamp(start.right() + delta.x, left + UIStyle::PANEL_MIN_WIDTH, bounds.right());
        if (edges & EDGE_TOP) top = std::clamp(start.y + delta.y, bounds.y, bottom - UIStyle::PANEL_MIN_HEIGHT);
        if (edges & EDGE_BOTTOM) bottom = std::clamp(start.bottom() + delta.y, top + UIStyle::PANEL_MIN_HEIGHT, bounds.bottom());

        return { left, top, right - left, bottom - top };
    }
}

void UIContext::beginPanel(std::string_view name, UIPanelState& state, const Rect& bounds, std::string_view title) {
    state.rect = clampInside(state.rect, bounds);

    // The region reaches past the panel by the grip width so edges can be grabbed from just outside
    const f32 grip = UIStyle::PANEL_RESIZE_GRIP;
    beginRegion({ state.rect.x - grip, state.rect.y - grip, state.rect.width + grip * 2.0f, state.rect.height + grip * 2.0f });

    // Resizing comes first so the top edge wins over the header drag
    const UIId resizeId = makeId(std::string(name) + "##resize");
    const u32 hoveredEdges = edgesUnderMouse(state.rect, m_input.mouse);
    Interaction resize;
    if (hoveredEdges != 0 || m_activeId == resizeId) {
        const Rect grab { state.rect.x - grip, state.rect.y - grip, state.rect.width + grip * 2.0f, state.rect.height + grip * 2.0f };
        resize = interact(resizeId, grab);
    }

    if (resize.activated) {
        m_resizeEdges = hoveredEdges;
        m_resizeStartRect = state.rect;
        m_resizeStartMouse = m_input.mouse;
    }
    if (resize.held) state.rect = resizeRect(m_resizeStartRect, m_resizeEdges, m_input.mouse - m_resizeStartMouse, bounds);

    if (resize.held) m_cursor = resizeCursor(m_resizeEdges);
    else if (resize.hovered) m_cursor = resizeCursor(hoveredEdges);

    const Rect header { state.rect.x, state.rect.y, state.rect.width, UIStyle::PANEL_HEADER_HEIGHT };
    const Interaction drag = interact(makeId(name), header);

    if (drag.activated) m_panelGrabOffset = m_input.mouse - Vec2(state.rect.x, state.rect.y);
    if (drag.held) {
        const Vec2 position = m_input.mouse - m_panelGrabOffset;
        state.rect = clampInside({ position.x, position.y, state.rect.width, state.rect.height }, bounds);
    }

    // Background, then a header strip with a line under it
    const Rect& panel = state.rect;
    UIDrawList& list = *m_drawList;

    list.popClip();
    list.shadow({ panel.x, panel.y + UIStyle::PANEL_SHADOW_OFFSET, panel.width, panel.height }, UIStyle::PANEL_RADIUS, UIStyle::PANEL_SHADOW_BLUR, UIStyle::PANEL_SHADOW);
    list.pushClip(panel);

    list.roundedRect(panel, UIStyle::PANEL_RADIUS, UIStyle::PANEL_BACKGROUND);

    const Rect movedHeader { panel.x, panel.y, panel.width, UIStyle::PANEL_HEADER_HEIGHT };
    list.pushClip(movedHeader);
    list.roundedRect({ panel.x, panel.y, panel.width, UIStyle::PANEL_HEADER_HEIGHT + UIStyle::PANEL_RADIUS }, UIStyle::PANEL_RADIUS,
                     drag.hovered || drag.held ? UIStyle::PANEL_HEADER_HOVER : UIStyle::PANEL_HEADER);
    list.popClip();
    list.rect({ panel.x, movedHeader.bottom() - 1.0f, panel.width, 1.0f }, UIStyle::PANEL_BORDER);
    list.roundedRect(panel, UIStyle::PANEL_RADIUS, { 0.0f, 0.0f, 0.0f, 0.0f }, resize.hovered || resize.held ? UIStyle::ACCENT : UIStyle::PANEL_BORDER, 1.0f);

    drawLabelText({ panel.x + UIStyle::PADDING, panel.y, panel.width, UIStyle::PANEL_HEADER_HEIGHT }, title, UIStyle::TEXT);

    // Widgets go below the header
    m_layout.area = { panel.x + UIStyle::PADDING, movedHeader.bottom() + UIStyle::PADDING,
                      panel.width - UIStyle::PADDING * 2.0f, panel.height - UIStyle::PANEL_HEADER_HEIGHT - UIStyle::PADDING * 2.0f };
    m_layout.cursorY = m_layout.area.y;
    m_layout.indent = 0.0f;

    pushId(name);
}

void UIContext::endPanel() {
    popId();
    endRegion();
}

void UIContext::pushId(std::string_view name) {
    m_idStack.push_back(makeId(name));
}

void UIContext::pushId(u32 number) {
    const u32 seed = m_idStack.empty() ? FNV_OFFSET : m_idStack.back();
    m_idStack.push_back(hashBytes(&number, sizeof(number), seed));
}

void UIContext::popId() {
    if (!m_idStack.empty()) m_idStack.pop_back();
}

UIId UIContext::makeId(std::string_view label) const {
    const u32 seed = m_idStack.empty() ? FNV_OFFSET : m_idStack.back();
    const UIId id = hashBytes(label.data(), label.size(), seed);
    return id == 0 ? 1 : id;   // 0 means "no widget"
}

void UIContext::spacing(f32 pixels) {
    m_layout.cursorY += pixels > 0.0f ? pixels : UIStyle::SECTION_SPACING;
}

void UIContext::separator() {
    const Rect row = nextRow(UIStyle::ITEM_SPACING * 2.0f + 1.0f);
    m_drawList->rect({ row.x, row.y + UIStyle::ITEM_SPACING, row.width, 1.0f }, UIStyle::SEPARATOR);
}

void UIContext::indent(f32 pixels) {
    m_layout.indent = std::max(0.0f, m_layout.indent + pixels);
}

Rect UIContext::nextRow(f32 height) {
    const f32 rowHeight = height > 0.0f ? height : UIStyle::ROW_HEIGHT;
    const Rect row { m_layout.area.x + m_layout.indent, m_layout.cursorY, m_layout.area.width - m_layout.indent, rowHeight };
    m_layout.cursorY += rowHeight + UIStyle::ITEM_SPACING;
    return row;
}

void UIContext::splitLabeledRow(const Rect& row, Rect& labelRect, Rect& controlRect) const {
    const f32 labelWidth = std::floor(row.width * UIStyle::LABEL_FRACTION);
    labelRect = { row.x, row.y, labelWidth, row.height };
    controlRect = { row.x + labelWidth, row.y, row.width - labelWidth, row.height };
}

void UIContext::drawLabelText(const Rect& rect, std::string_view text, const Color& color) {
    const f32 y = rect.y + std::floor((rect.height - m_font.glyphHeight) * 0.5f);
    m_drawList->text(Vec2(rect.x, y), text, m_font, color);
}

UIContext::Interaction UIContext::interact(UIId id, const Rect& rect) {
    Interaction result;

    const bool inRegion = m_currentRegion >= 0 && m_currentRegion == m_hoveredRegion;
    result.hovered = m_interactive && inRegion && rect.contains(m_input.mouse) && (m_activeId == 0 || m_activeId == id);
    if (result.hovered) m_hotId = id;

    if (result.hovered && m_activeId == 0 && m_input.pressed[UIInput::LEFT]) {
        m_activeId = id;
        result.activated = true;
    }

    if (m_activeId == id) {
        m_activeSeen = true;

        if (m_input.released[UIInput::LEFT] || !m_input.down[UIInput::LEFT]) {
            result.deactivated = true;
            result.clicked = rect.contains(m_input.mouse);
            m_activeId = 0;
        } else {
            result.held = true;
        }
    }

    return result;
}

void UIContext::finishItem(const Interaction& interaction, bool changed) {
    if (interaction.activated) m_editedSinceActivation = false;
    if (changed) m_editedSinceActivation = true;

    m_last.hovered = interaction.hovered;
    m_last.activated = interaction.activated;
    m_last.deactivated = interaction.deactivated;
    m_last.deactivatedAfterEdit = interaction.deactivated && m_editedSinceActivation;
}
