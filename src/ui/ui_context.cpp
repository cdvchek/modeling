#include "ui/ui_context.hpp"
#include "ui/ui_style.hpp"

#include <algorithm>
#include <iostream>
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

    if (!m_interactive) {
        m_activeId = 0;
        m_textEdit = {};
        m_drag = {};
    }

    // A drag ends on the release; drop targets drawn this frame can take it
    if (m_drag.active && (m_input.released[UIInput::LEFT] || !m_input.down[UIInput::LEFT])) m_drag.dropping = true;

    // Topmost region from last frame under the mouse
    m_hoveredRegion = -1;
    for (i32 i = static_cast<i32>(m_previousRegions.size()) - 1; i >= 0; --i) {
        if (m_previousRegions[i].contains(m_input.mouse)) {
            m_hoveredRegion = i;
            break;
        }
    }

    // While a dropdown list is open, a press anywhere else just closes it
    if (m_popup.open && m_input.anyPressed()) {
        const bool overPopup = m_hoveredRegion >= 0 && m_hoveredRegion == m_previousPopupRegion;
        if (!overPopup && !m_popup.anchor.contains(m_input.mouse)) {
            m_popup.open = false;
            for (u32 i = 0; i < UIInput::BUTTON_COUNT; ++i) m_input.pressed[i] = false;
            m_wantsMouse = m_interactive;
            m_hoveredRegion = -1;
            return;
        }
    }

    // A press that starts outside the UI belongs to the viewport until every button is released
    if (m_input.anyPressed() && m_hoveredRegion < 0 && m_activeId == 0) m_viewportOwnsMouse = true;
    if (!m_input.anyDown() && !m_input.anyPressed()) m_viewportOwnsMouse = false;
    if (m_viewportOwnsMouse) m_hoveredRegion = -1;

    // A click that ends a text edit only ends it; it doesn't also reach the viewport
    const bool endsTextEdit = m_textEdit.id != 0 && m_input.anyPressed() && !m_textEdit.rect.contains(m_input.mouse);

    m_wantsMouse = m_interactive && (m_hoveredRegion >= 0 || m_activeId != 0 || m_popup.open || endsTextEdit);
}

void UIContext::beginDraw() {
    m_popupOwnerSeen = false;
    m_cursor = UICursor::Arrow;
    m_seenIds.clear();
    m_hotId = 0;
    m_activeSeen = false;
    m_regions.clear();
    m_idStack.clear();
    m_currentRegion = -1;
    m_last = {};
    m_textEdit.seen = false;
}

void UIContext::endDraw() {
    drawPopup();
    drawDragLabel();
    if (m_drag.dropping) m_drag = {};
    m_previousRegions = m_regions;

    // A widget that stopped being drawn mid-drag (e.g. its light was deleted) can't hold the mouse forever
    if (!m_activeSeen || !m_input.down[UIInput::LEFT]) m_activeId = 0;

    // Same for a text field that went away mid-edit (e.g. another tab): the edit is dropped
    if (m_textEdit.id != 0 && !m_textEdit.seen) m_textEdit = {};

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
    beginPanelHeader(name, state, bounds, { title }, false);
}

void UIContext::beginPanel(std::string_view name, UIPanelState& state, const Rect& bounds, const std::vector<std::string_view>& tabs) {
    beginPanelHeader(name, state, bounds, tabs, true);
}

void UIContext::beginPanelHeader(std::string_view name, UIPanelState& state, const Rect& bounds, const std::vector<std::string_view>& tabs, bool showTabs) {
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

    // Tabs sit in the header, left to right, sized to their text
    std::vector<Rect> tabRects;
    if (showTabs) {
        f32 x = state.rect.x + UIStyle::TAB_INSET;
        for (std::string_view tab : tabs) {
            const f32 width = measureText(m_font, tab).x + UIStyle::TAB_PADDING * 2.0f;
            tabRects.push_back({ x, state.rect.y + UIStyle::TAB_INSET, width, UIStyle::PANEL_HEADER_HEIGHT - UIStyle::TAB_INSET });
            x += width;
        }
    }

    // Pressing a tab switches to it; the header (tabs included) still drags the panel
    if (drag.activated) {
        m_panelGrabOffset = m_input.mouse - Vec2(state.rect.x, state.rect.y);
        for (u32 i = 0; i < tabRects.size(); ++i) {
            if (tabRects[i].contains(m_input.mouse) && state.activeTab != static_cast<i32>(i)) {
                state.activeTab = static_cast<i32>(i);
                state.scroll = 0.0f;
            }
        }
    }
    if (!tabs.empty()) state.activeTab = std::clamp(state.activeTab, 0, static_cast<i32>(tabs.size()) - 1);
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

    if (showTabs) {
        // Rects were laid out before a drag moved the panel this frame, so shift them along
        const Vec2 moved(panel.x - (tabRects.empty() ? panel.x : tabRects.front().x - UIStyle::TAB_INSET), 0.0f);
        const f32 dy = panel.y - (tabRects.empty() ? panel.y : tabRects.front().y - UIStyle::TAB_INSET);

        for (u32 i = 0; i < tabs.size(); ++i) {
            const Rect tab { tabRects[i].x + moved.x, tabRects[i].y + dy, tabRects[i].width, tabRects[i].height };
            const bool selected = state.activeTab == static_cast<i32>(i);
            const bool hovered = drag.hovered && tab.contains(m_input.mouse);

            // The selected tab takes the body's color so it reads as connected to the content, with an accent underline
            if (selected) {
                list.pushClip(tab);
                list.roundedRect({ tab.x, tab.y, tab.width, tab.height + UIStyle::CORNER_RADIUS }, UIStyle::CORNER_RADIUS, UIStyle::PANEL_BACKGROUND);
                list.popClip();
                list.rect({ tab.x, tab.bottom() - UIStyle::TAB_UNDERLINE, tab.width, UIStyle::TAB_UNDERLINE }, UIStyle::ACCENT);
            }

            const f32 textWidth = measureText(m_font, tabs[i]).x;
            drawLabelText({ tab.x + std::floor((tab.width - textWidth) * 0.5f), tab.y, textWidth, tab.height }, tabs[i],
                          selected || hovered ? UIStyle::TEXT : UIStyle::TEXT_DIM);
        }
    } else if (!tabs.empty()) {
        drawLabelText({ panel.x + UIStyle::PADDING, panel.y, panel.width, UIStyle::PANEL_HEADER_HEIGHT }, tabs.front(), UIStyle::TEXT);
    }

    // Widgets go below the header; content scrolls inside the area under it
    m_layout.area = { panel.x + UIStyle::PADDING, movedHeader.bottom() + UIStyle::PADDING,
                      panel.width - UIStyle::PADDING * 2.0f, panel.height - UIStyle::PANEL_HEADER_HEIGHT - UIStyle::PADDING * 2.0f };
    m_layout.indent = 0.0f;

    m_contentRect = { panel.x, movedHeader.bottom(), panel.width, panel.bottom() - movedHeader.bottom() };

    state.scroll = std::clamp(state.scroll, 0.0f, std::max(0.0f, state.contentHeight - m_layout.area.height));

    m_layout.cursorY = m_layout.area.y - state.scroll;

    m_panel = &state;
    list.pushClip(m_contentRect);
    m_hasContentClip = true;

    pushId(name);
}

void UIContext::endPanel() {
    popId();

    UIPanelState& state = *m_panel;
    UIDrawList& list = *m_drawList;

    // Content height = everything laid out since the top, without the trailing spacing
    const f32 contentTop = m_layout.area.y - state.scroll;
    state.contentHeight = std::max(0.0f, m_layout.cursorY - UIStyle::ITEM_SPACING - contentTop);

    list.popClip();
    m_hasContentClip = false;

    // The wheel goes to the panel unless a child box under the mouse already took it this frame
    const f32 visible = m_layout.area.height;
    const f32 maxScroll = std::max(0.0f, state.contentHeight - visible);
    const bool panelHovered = m_interactive && m_currentRegion == m_hoveredRegion && m_activeId == 0;
    if (panelHovered && m_input.scroll != 0) {
        state.scroll += wheelScroll();
        m_input.scroll = 0;
    }
    state.scroll = std::clamp(state.scroll, 0.0f, maxScroll);

    // Scrollbar in the right padding, only when there's more content than fits
    if (maxScroll > 0.0f) {
        const Rect track { state.rect.right() - UIStyle::SCROLLBAR_INSET - UIStyle::SCROLLBAR_WIDTH, m_layout.area.y, UIStyle::SCROLLBAR_WIDTH, visible };
        scrollbar(makeId("##scrollbar"), track, state.scroll, state.contentHeight);
    }

    m_panel = nullptr;
    endRegion();
}

void UIContext::beginModal(std::string_view name, const Rect& viewport, f32 width, f32 height, std::string_view title) {
    beginRegion(viewport);
    UIDrawList& list = *m_drawList;
    list.rect(viewport, UIStyle::MODAL_BACKDROP);

    const Rect window = clampInside({ std::floor(viewport.x + (viewport.width - width) * 0.5f), std::floor(viewport.y + (viewport.height - height) * 0.5f), width, height }, viewport);
    const Rect header { window.x, window.y, window.width, UIStyle::PANEL_HEADER_HEIGHT };

    list.shadow({ window.x, window.y + UIStyle::PANEL_SHADOW_OFFSET, window.width, window.height }, UIStyle::PANEL_RADIUS, UIStyle::PANEL_SHADOW_BLUR, UIStyle::PANEL_SHADOW);
    list.roundedRect(window, UIStyle::PANEL_RADIUS, UIStyle::PANEL_BACKGROUND);
    list.pushClip(header);
    list.roundedRect({ window.x, window.y, window.width, header.height + UIStyle::PANEL_RADIUS }, UIStyle::PANEL_RADIUS, UIStyle::PANEL_HEADER);
    list.popClip();
    list.rect({ window.x, header.bottom() - 1.0f, window.width, 1.0f }, UIStyle::PANEL_BORDER);
    list.roundedRect(window, UIStyle::PANEL_RADIUS, { 0.0f, 0.0f, 0.0f, 0.0f }, UIStyle::PANEL_BORDER, 1.0f);
    drawLabelText({ window.x + UIStyle::PADDING, window.y, window.width, header.height }, title, UIStyle::ACCENT_GREEN);

    m_layout.area = { window.x + UIStyle::PADDING, header.bottom() + UIStyle::PADDING, window.width - UIStyle::PADDING * 2.0f, window.bottom() - header.bottom() - UIStyle::PADDING * 2.0f };
    m_layout.cursorY = m_layout.area.y;
    m_layout.indent = 0.0f;

    // Widgets are only reachable inside the window; the backdrop just absorbs clicks
    m_contentRect = { window.x, header.bottom(), window.width, window.bottom() - header.bottom() };
    m_hasContentClip = true;
    list.pushClip(m_contentRect);
    pushId(name);
}

void UIContext::endModal() {
    popId();
    m_drawList->popClip();
    m_hasContentClip = false;
    endRegion();
}

// Pixels to scroll for this frame's wheel input; wheel up (positive) scrolls toward the top
f32 UIContext::wheelScroll() const {
    return -(static_cast<f32>(m_input.scroll) / 120.0f) * UIStyle::SCROLL_STEP;
}

void UIContext::scrollbar(UIId id, const Rect& track, f32& scroll, f32 contentHeight) {
    const f32 maxScroll = std::max(0.0f, contentHeight - track.height);
    if (maxScroll <= 0.0f) return;

    const f32 thumbHeight = std::max(UIStyle::SCROLLBAR_MIN_THUMB, track.height * track.height / contentHeight);
    const f32 travel = track.height - thumbHeight;
    const Rect thumb { track.x, track.y + travel * (scroll / maxScroll), track.width, thumbHeight };

    // A slightly wider hit area makes the thin thumb easy to grab
    const Rect thumbHit { thumb.x - 4.0f, thumb.y, thumb.width + 8.0f, thumb.height };
    const Interaction drag = interact(id, thumbHit);
    if (drag.activated) m_scrollGrabOffset = m_input.mouse.y - thumb.y;
    if (drag.held && travel > 0.0f) {
        const f32 t = std::clamp((m_input.mouse.y - m_scrollGrabOffset - track.y) / travel, 0.0f, 1.0f);
        scroll = t * maxScroll;
    }

    const Rect drawnThumb { track.x, track.y + travel * (scroll / maxScroll), track.width, thumbHeight };
    m_drawList->roundedRect(track, UIStyle::SCROLLBAR_WIDTH * 0.5f, UIStyle::SCROLLBAR_TRACK);
    m_drawList->roundedRect(drawnThumb, UIStyle::SCROLLBAR_WIDTH * 0.5f, drag.hovered || drag.held ? UIStyle::SCROLLBAR_THUMB_HOVER : UIStyle::SCROLLBAR_THUMB);
}

Rect UIContext::beginChild(std::string_view name, f32 height) {
    const Rect rect = nextRow(height);
    const UIId id = makeId(name);
    ChildState& state = m_children[id];

    m_drawList->roundedRect(rect, UIStyle::CORNER_RADIUS, UIStyle::LIST_BACKGROUND, UIStyle::FRAME_BORDER, 1.0f);

    // Room on the right for the scrollbar
    const f32 pad = UIStyle::CHILD_PADDING;
    const Rect inner { rect.x + pad, rect.y + pad, rect.width - pad * 2.0f - UIStyle::SCROLLBAR_WIDTH - pad, rect.height - pad * 2.0f };

    // The wheel over the box scrolls it and is used up, so the panel doesn't scroll too
    const bool visible = !m_hasContentClip || m_contentRect.contains(m_input.mouse);
    const bool hovered = m_interactive && m_currentRegion == m_hoveredRegion && visible && rect.contains(m_input.mouse) && m_activeId == 0;
    const f32 maxScroll = std::max(0.0f, state.contentHeight - inner.height);
    if (hovered && m_input.scroll != 0 && maxScroll > 0.0f) {
        state.scroll += wheelScroll();
        m_input.scroll = 0;
    }
    state.scroll = std::clamp(state.scroll, 0.0f, maxScroll);

    m_childStack.push_back({ id, rect, inner, m_layout, m_contentRect, m_hasContentClip });

    m_layout.area = inner;
    m_layout.cursorY = inner.y - state.scroll;
    m_layout.indent = 0.0f;

    const Rect clip { rect.x + 1.0f, rect.y + 1.0f, rect.width - 2.0f, rect.height - 2.0f };
    m_drawList->pushClip(clip);
    m_contentRect = m_hasContentClip ? Rect::intersect(m_contentRect, clip) : clip;
    m_hasContentClip = true;

    pushId(name);
    return rect;
}

void UIContext::endChild() {
    popId();

    const ChildFrame frame = m_childStack.back();
    m_childStack.pop_back();
    ChildState& state = m_children[frame.id];

    state.contentHeight = std::max(0.0f, m_layout.cursorY - UIStyle::ITEM_SPACING - (frame.inner.y - state.scroll));
    state.scroll = std::clamp(state.scroll, 0.0f, std::max(0.0f, state.contentHeight - frame.inner.height));

    m_drawList->popClip();
    m_layout = frame.parentLayout;
    m_contentRect = frame.parentContentRect;
    m_hasContentClip = frame.parentHadContentClip;

    const Rect track { frame.rect.right() - UIStyle::CHILD_PADDING - UIStyle::SCROLLBAR_WIDTH, frame.inner.y, UIStyle::SCROLLBAR_WIDTH, frame.inner.height };
    pushId(frame.id);
    scrollbar(makeId("##scrollbar"), track, state.scroll, state.contentHeight);
    popId();
}

bool UIContext::mouseIn(const Rect& rect) const {
    const bool visible = !m_hasContentClip || m_contentRect.contains(m_input.mouse);
    return m_interactive && visible && rect.contains(m_input.mouse);
}

bool UIContext::dragSource(u32 payload, std::string_view label) {
    if (m_drag.active) return m_drag.payload == payload;
    if (m_activeId == 0 || m_activeId != m_lastItemId || m_activeTravel < 3.0f || !m_input.down[UIInput::LEFT]) return false;

    m_drag.active = true;
    m_drag.payload = payload;
    m_drag.label = std::string(label);
    return true;
}

bool UIContext::acceptDrop(const Rect& rect, u32& payload) {
    if (!m_drag.dropping || !mouseIn(rect)) return false;
    payload = m_drag.payload;
    m_drag.dropping = false;
    m_drag.active = false;
    return true;
}

void UIContext::drawDragLabel() {
    if (!m_drag.active || m_drag.dropping || !m_drawList) return;

    // A small tag just below and right of the cursor, over everything
    const Vec2 size = measureText(m_font, m_drag.label);
    const Rect tag { m_input.mouse.x + 14.0f, m_input.mouse.y + 10.0f, size.x + UIStyle::TEXT_PADDING * 2.0f, UIStyle::ROW_HEIGHT };
    m_drawList->shadow({ tag.x, tag.y + 2.0f, tag.width, tag.height }, UIStyle::CORNER_RADIUS, 6.0f, UIStyle::PANEL_SHADOW);
    m_drawList->roundedRect(tag, UIStyle::CORNER_RADIUS, UIStyle::PANEL_BACKGROUND, UIStyle::ACCENT, 1.0f);
    drawLabelText({ tag.x + UIStyle::TEXT_PADDING, tag.y, size.x, tag.height }, m_drag.label, UIStyle::TEXT);
}

void UIContext::drawPopup() {
    m_previousPopupRegion = -1;
    if (!m_popup.open) return;

    // Its dropdown wasn't drawn this frame (e.g. another tab), so the list goes away with it
    if (!m_popupOwnerSeen) {
        m_popup.open = false;
        return;
    }

    const u32 count = static_cast<u32>(m_popup.options.size());
    f32 widest = 0.0f;
    for (const std::string& option : m_popup.options) widest = std::max(widest, measureText(m_font, option).x);

    const f32 pad = UIStyle::POPUP_PADDING;
    const f32 width = std::max(m_popup.anchor.width, widest + UIStyle::TEXT_PADDING * 2.0f + pad * 2.0f);
    const f32 height = count * UIStyle::ROW_HEIGHT + pad * 2.0f;

    // Below the button if it fits, otherwise above, and never past the viewport's sides
    Rect rect { m_popup.anchor.x, m_popup.anchor.bottom() + 2.0f, width, height };
    if (rect.bottom() > m_viewport.bottom()) rect.y = m_popup.anchor.y - 2.0f - height;
    rect.x = std::clamp(rect.x, m_viewport.x, std::max(m_viewport.x, m_viewport.right() - width));

    m_currentRegion = static_cast<i32>(m_regions.size());
    m_previousPopupRegion = m_currentRegion;
    m_regions.push_back(rect);
    m_hasContentClip = false;

    UIDrawList& list = *m_drawList;
    list.shadow({ rect.x, rect.y + 3.0f, rect.width, rect.height }, UIStyle::CORNER_RADIUS, 8.0f, UIStyle::PANEL_SHADOW);
    list.roundedRect(rect, UIStyle::CORNER_RADIUS, UIStyle::PANEL_BACKGROUND, UIStyle::PANEL_BORDER, 1.0f);

    pushId(m_popup.owner);
    for (u32 i = 0; i < count; ++i) {
        const Rect row { rect.x + pad, rect.y + pad + i * UIStyle::ROW_HEIGHT, rect.width - pad * 2.0f, UIStyle::ROW_HEIGHT };
        const Interaction interaction = interact(makeId(m_popup.options[i]), row);

        const bool current = static_cast<i32>(i) == m_popup.current;
        if (interaction.hovered || interaction.held) list.roundedRect(row, UIStyle::CORNER_RADIUS, UIStyle::ACCENT_SOFT);
        drawLabelText({ row.x + UIStyle::TEXT_PADDING, row.y, row.width, row.height }, m_popup.options[i], current ? UIStyle::ACCENT : UIStyle::TEXT);

        if (interaction.clicked) m_popup.picked = static_cast<i32>(i);
    }
    popId();

    m_currentRegion = -1;
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
    m_lastLabel = label;
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

    m_lastItemId = id;
    m_lastRect = rect;

    if (!m_seenIds.insert(id).second && m_reportedDuplicates.insert(id).second) {
        std::cerr << "[ui] two widgets share ID " << id << " (" << m_lastLabel << "); wrap one in pushId/popId" << std::endl;
    }

    // Widgets scrolled out of the content area can't be hovered through the header or panel edge
    const bool inRegion = m_currentRegion >= 0 && m_currentRegion == m_hoveredRegion;
    const bool visible = !m_hasContentClip || m_contentRect.contains(m_input.mouse);
    result.hovered = m_interactive && inRegion && visible && rect.contains(m_input.mouse) && (m_activeId == 0 || m_activeId == id);
    if (result.hovered) m_hotId = id;

    if (result.hovered && m_activeId == 0 && m_input.pressed[UIInput::LEFT]) {
        m_activeId = id;
        m_activeTravel = 0.0f;
        result.activated = true;
    }

    if (m_activeId == id) {
        m_activeSeen = true;
        if (!result.activated) m_activeTravel += m_input.mouseDelta.length();

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
