#pragma once

#include <string_view>
#include <unordered_set>
#include <vector>
#include "core/math/vec3.hpp"
#include "ui/ui_draw_list.hpp"
#include "ui/ui_input.hpp"

using UIId = u32;

// The mouse cursor the UI would like shown this frame
enum class UICursor : u8 {
    Arrow,
    ResizeHorizontal,
    ResizeVertical,
    ResizeDiagonalDown,   // top-left to bottom-right
    ResizeDiagonalUp      // bottom-left to top-right
};

// Kept by the app between frames; a width of 0 means "place it on first use"
struct UIPanelState {
    Rect rect;
};

// Immediate-mode UI: each widget call draws itself and handles its input in the same place
class UIContext {
public:
    // Call before the app handles input; interactive is false while the console or a modal tool owns input
    void beginFrame(const UIInput& input, bool interactive);

    // Brackets the widget calls; can run more than once per frame (e.g. redraws while resizing)
    void beginDraw();
    void endDraw();

    // True when the mouse is over UI or a widget is being dragged; the viewport should ignore the mouse then
    bool wantsMouse() const { return m_wantsMouse; }

    void setDrawList(UIDrawList* list) { m_drawList = list; }
    void setFont(const UIFont& font) { m_font = font; }
    UIDrawList& drawList() { return *m_drawList; }
    const UIFont& font() const { return m_font; }

    // A region is an area that blocks the viewport (a panel); widgets are laid out top to bottom inside it
    void beginRegion(const Rect& rect);
    void endRegion();

    // A floating panel: drag the header to move it, drag an edge or corner to resize it; always kept inside bounds
    void beginPanel(std::string_view name, UIPanelState& state, const Rect& bounds, std::string_view title);
    void endPanel();

    // Widgets with the same label need different scopes (e.g. one per light)
    void pushId(std::string_view name);
    void pushId(u32 number);
    void popId();
    UIId makeId(std::string_view label) const;

    // Layout
    void spacing(f32 pixels = 0.0f);
    void separator();
    void indent(f32 pixels);

    // Widgets; each returns true when it changed the value (or was clicked)
    void label(std::string_view text, bool dim = false);
    void heading(std::string_view text);
    bool button(std::string_view label);
    bool selectable(std::string_view label, bool selected);
    bool checkbox(std::string_view label, bool& value);
    bool sliderFloat(std::string_view label, f32& value, f32 min, f32 max, const char* format = "%.2f");
    bool dragFloat3(std::string_view label, Vec3& value, f32 speed, const char* format = "%.2f");
    bool colorEdit(std::string_view label, Vec3& color);

    // About the widget just drawn; values never change on the activation frame, so undo can begin there
    bool isItemHovered() const { return m_last.hovered; }
    bool isItemActivated() const { return m_last.activated; }
    bool isItemDeactivated() const { return m_last.deactivated; }
    bool isItemDeactivatedAfterEdit() const { return m_last.deactivatedAfterEdit; }

    UICursor cursor() const { return m_cursor; }

    UIId getActiveId() const { return m_activeId; }
    UIId getHotId() const { return m_hotId; }

private:
    struct Interaction {
        bool hovered = false;
        bool activated = false;
        bool held = false;
        bool deactivated = false;
        bool clicked = false;
    };

    struct ItemState {
        bool hovered = false;
        bool activated = false;
        bool deactivated = false;
        bool deactivatedAfterEdit = false;
    };

    struct Layout {
        Rect area;
        f32 cursorY = 0.0f;
        f32 indent = 0.0f;
    };

    Interaction interact(UIId id, const Rect& rect);
    void finishItem(const Interaction& interaction, bool changed);

    Rect nextRow(f32 height = 0.0f);
    void splitLabeledRow(const Rect& row, Rect& labelRect, Rect& controlRect) const;
    void drawLabelText(const Rect& rect, std::string_view text, const Color& color);
    bool sliderControl(UIId id, const Rect& rect, f32& value, f32 min, f32 max, const char* format, const Color* fillColor, Interaction& interaction);

    UIInput m_input;
    bool m_interactive = false;
    bool m_wantsMouse = false;
    bool m_viewportOwnsMouse = false;

    UIId m_hotId = 0;
    UIId m_activeId = 0;
    bool m_editedSinceActivation = false;
    bool m_activeSeen = false;
    ItemState m_last;

    // Regions drawn last frame are what the mouse is tested against this frame
    std::vector<Rect> m_previousRegions;
    std::vector<Rect> m_regions;
    i32 m_hoveredRegion = -1;
    i32 m_currentRegion = -1;

    std::vector<UIId> m_idStack;
    Layout m_layout;

    std::unordered_set<UIId> m_openWidgets;

    // Where in the header the mouse grabbed the panel, so it doesn't jump when dragging starts
    Vec2 m_panelGrabOffset;

    // Edges being resized (bit flags) and the panel and mouse when the resize started
    u32 m_resizeEdges = 0;
    Rect m_resizeStartRect;
    Vec2 m_resizeStartMouse;

    UICursor m_cursor = UICursor::Arrow;

    UIDrawList* m_drawList = nullptr;
    UIFont m_font;
};
