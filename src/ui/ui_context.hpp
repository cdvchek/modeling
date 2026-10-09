#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
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
    ResizeDiagonalUp,     // bottom-left to top-right
    Text                  // I-beam over a text field
};

// Kept by the app between frames; a width of 0 means "place it on first use"
struct UIPanelState {
    Rect rect;
    i32 activeTab = 0;
    f32 scroll = 0.0f;          // how far the content is scrolled down, in pixels
    f32 contentHeight = 0.0f;   // measured at endPanel; used to limit scrolling next frame
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

    // True while a text field is being edited; keyboard shortcuts should stay quiet then
    bool wantsKeyboard() const { return m_textEdit.id != 0; }

    // How text fields reach the system clipboard; without it, copy and paste do nothing
    void setClipboard(std::function<std::string()> get, std::function<void(const std::string&)> set) {
        m_getClipboard = std::move(get);
        m_setClipboard = std::move(set);
    }

    void setDrawList(UIDrawList* list) { m_drawList = list; }
    void setFont(const UIFont& font) { m_font = font; }
    UIDrawList& drawList() { return *m_drawList; }
    const UIFont& font() const { return m_font; }

    // A region is an area that blocks the viewport (a panel); widgets are laid out top to bottom inside it
    void beginRegion(const Rect& rect);
    void endRegion();

    // A floating panel: drag the header to move it, drag an edge or corner to resize it; always kept inside bounds
    void beginPanel(std::string_view name, UIPanelState& state, const Rect& bounds, std::string_view title);

    // Same, with tabs in the header instead of a title; pressing a tab switches state.activeTab
    void beginPanel(std::string_view name, UIPanelState& state, const Rect& bounds, const std::vector<std::string_view>& tabs);
    void endPanel();

    // A window centered in viewport over a dimmed backdrop. The backdrop is a region covering the whole viewport,
    // so while it's drawn (last, after every panel) nothing under it can be hovered or clicked. Widgets go inside
    // as in a panel. The window isn't moved or resized; height is its full height, header included.
    void beginModal(std::string_view name, const Rect& viewport, f32 width, f32 height, std::string_view title);
    void endModal();

    // Widgets with the same label need different scopes (e.g. one per light)
    void pushId(std::string_view name);
    void pushId(u32 number);
    void popId();
    UIId makeId(std::string_view label) const;

    // A fixed-height box that scrolls its own content (wheel over it scrolls the box, not the panel); returns the box
    Rect beginChild(std::string_view name, f32 height);
    void endChild();

    // Layout
    Rect row(f32 height = 0.0f) { return nextRow(height); }
    void text(const Rect& rect, std::string_view text, const Color& color) { drawLabelText(rect, text, color); }
    void spacing(f32 pixels = 0.0f);
    void separator();
    void indent(f32 pixels);

    // Widgets; each returns true when it changed the value (or was clicked)
    void label(std::string_view text, bool dim = false);
    void heading(std::string_view text);
    bool button(std::string_view label);
    bool button(std::string_view label, const Rect& rect, bool enabled = true);
    // A tab in a bar of tabs: the selected one takes the panel's color with an accent underline; true when an
    // unselected tab is clicked
    bool tab(std::string_view label, const Rect& rect, bool selected);
    // A vertical divider dragged left and right: x moves with the mouse while it's held; true when it moved
    bool splitter(std::string_view id, const Rect& rect, f32& x);
    bool segmented(std::string_view label, i32& index, const std::vector<std::string_view>& options);
    // Same control in a given rect, no label; id tells same-looking switches apart
    bool segmented(std::string_view id, const Rect& rect, i32& index, const std::vector<std::string_view>& options);

    // A button showing options[index]; clicking it opens a list on top of everything to pick from
    // icons, when given, are textures (one per option, 0 for none) drawn as small squares before each label
    bool dropdown(std::string_view label, const Rect& rect, i32& index, const std::vector<std::string_view>& options,
                  const std::vector<u32>& icons = {});

    // Area popups are kept inside (the viewport)
    void setViewport(const Rect& viewport) { m_viewport = viewport; }   // empty label = full row
    // detail is dim, right-aligned; icon, a texture drawn as a small square before the label
    bool selectable(std::string_view label, bool selected, std::string_view detail = {}, u32 icon = 0);
    // A selectable row in a tree: indented by depth, with an arrow that folds its children away (open toggles
    // on a click on the arrow) when it has any. Returns true when the row itself is clicked.
    bool treeRow(std::string_view label, bool selected, std::string_view detail, u32 depth, bool hasChildren, bool& open);

    // Drag and drop. Right after a widget, dragSource starts a drag carrying payload once the widget has been
    // pressed and moved past the drag threshold; a label follows the mouse while it lasts. On the release,
    // the first acceptDrop whose rect is under the mouse takes the payload; the drag then ends either way.
    bool dragSource(u32 payload, std::string_view label);
    bool acceptDrop(const Rect& rect, u32& payload);
    bool isDragging() const { return m_drag.active; }
    // A dropdown's list is open (it sits over everything and takes the mouse)
    bool popupOpen() const { return m_popup.open; }
    u32 dragPayload() const { return m_drag.payload; }
    // The rect of the last widget that handled the mouse, e.g. to outline a row as a drop target
    const Rect& lastItemRect() const { return m_lastRect; }
    bool mouseIn(const Rect& rect) const;
    bool checkbox(std::string_view label, bool& value);
    // Just the box, at the left of rect and centered in its height; the whole rect is clickable
    bool checkbox(std::string_view id, const Rect& rect, bool& value);
    bool sliderFloat(std::string_view label, f32& value, f32 min, f32 max, const char* format = "%.2f");
    // Drag a component left/right to change it; click it without dragging to type a value
    bool dragFloat3(std::string_view label, Vec3& value, f32 speed, const char* format = "%.2f");
    // One value, dragged or typed the same way
    bool dragFloat(std::string_view label, f32& value, f32 speed, const char* format = "%.2f");

    // Click to edit (all text selected). Enter or a click anywhere else keeps the edit, Escape restores the old text.
    // Returns true once, when an edit is kept that changed the text; without allowEmpty, an empty edit is dropped
    bool textField(std::string_view label, std::string& text, bool allowEmpty = false);
    // A swatch; clicking it opens a picker under it: a saturation/value square and hue strip, a hex field
    // (#rrggbb), and typed R/G/B values. While open, the swatch shows the color it opened with beside the new one.
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

    enum class TextEditResult : u8 {
        Editing,
        Committed,
        Cancelled
    };

    // The one text field being edited, if any; widgets draw it with textEditBox instead of their usual look
    struct TextEdit {
        UIId id = 0;
        std::string buffer;
        u32 caret = 0;
        u32 anchor = 0;             // the selection runs between anchor and caret
        f32 scroll = 0.0f;          // pixels the text is shifted left to keep the caret in view
        Rect rect;                  // where it was last drawn
        f64 lastInput = 0.0;        // the caret stays solid for a moment after each edit
        bool seen = false;          // drawn this frame
        bool dragging = false;      // the mouse is selecting text
        bool justStarted = false;   // the click that started editing isn't also a caret click
    };

    void beginTextEdit(UIId id, const std::string& text);
    // Runs and draws the field being edited; on Committed, committed holds the text
    TextEditResult textEditBox(const Rect& rect, std::string& committed);

    Interaction interact(UIId id, const Rect& rect);
    void finishItem(const Interaction& interaction, bool changed);

    Rect nextRow(f32 height = 0.0f);
    void splitLabeledRow(const Rect& row, Rect& labelRect, Rect& controlRect) const;
    void drawLabelText(const Rect& rect, std::string_view text, const Color& color);
    bool sliderControl(UIId id, const Rect& rect, f32& value, f32 min, f32 max, const char* format, const Color* fillColor, Interaction& interaction);
    void scrollbar(UIId id, const Rect& track, f32& scroll, f32 contentHeight);
    f32 wheelScroll() const;

    UIInput m_input;
    bool m_interactive = false;
    bool m_wantsMouse = false;
    bool m_viewportOwnsMouse = false;

    UIId m_hotId = 0;
    UIId m_activeId = 0;
    bool m_editedSinceActivation = false;
    bool m_activeSeen = false;
    ItemState m_last;
    UIId m_lastItemId = 0;
    Rect m_lastRect;

    struct Drag {
        bool active = false;
        bool dropping = false;   // released this frame; acceptDrop can take it until endDraw
        u32 payload = 0;
        std::string label;
    };
    Drag m_drag;
    void drawDragLabel();

    // How far the mouse has moved since the active widget was pressed; under the threshold, a release is a click
    f32 m_activeTravel = 0.0f;

    TextEdit m_textEdit;
    std::function<std::string()> m_getClipboard;
    std::function<void(const std::string&)> m_setClipboard;

    // Regions drawn last frame are what the mouse is tested against this frame
    std::vector<Rect> m_previousRegions;
    std::vector<Rect> m_regions;
    i32 m_hoveredRegion = -1;
    i32 m_currentRegion = -1;

    std::vector<UIId> m_idStack;
    Layout m_layout;

    std::unordered_set<UIId> m_openWidgets;

    // Open color pickers: the color each opened with, and the hue kept while the color is gray or black
    struct ColorPicker {
        f32 hue = 0.0f;
        f32 saturation = 0.0f;
        f32 value = 0.0f;
        Vec3 opened;
    };
    std::unordered_map<UIId, ColorPicker> m_colorPickers;

    // Where in the header the mouse grabbed the panel, so it doesn't jump when dragging starts
    Vec2 m_panelGrabOffset;

    // Edges being resized (bit flags) and the panel and mouse when the resize started
    u32 m_resizeEdges = 0;
    Rect m_resizeStartRect;
    Vec2 m_resizeStartMouse;

    UICursor m_cursor = UICursor::Arrow;

    // Two widgets with the same ID in one frame would act as one; each duplicate is reported once
    std::unordered_set<UIId> m_seenIds;
    std::unordered_set<UIId> m_reportedDuplicates;
    mutable std::string m_lastLabel;   // for the duplicate warning

    // The panel being drawn: its state, the visible content area, and where its content starts
    UIPanelState* m_panel = nullptr;
    Rect m_contentRect;
    bool m_hasContentClip = false;
    f32 m_scrollGrabOffset = 0.0f;

    struct ChildState {
        f32 scroll = 0.0f;
        f32 contentHeight = 0.0f;
    };

    // What a child box replaced, restored at endChild
    struct ChildFrame {
        UIId id;
        Rect rect;
        Rect inner;
        Layout parentLayout;
        Rect parentContentRect;
        bool parentHadContentClip;
    };

    // One dropdown list can be open at a time; it's drawn last (on top) in endDraw
    struct Popup {
        bool open = false;
        UIId owner = 0;
        Rect anchor;
        std::vector<std::string> options;
        std::vector<u32> icons;
        i32 current = 0;
        i32 picked = -1;   // read by the owning dropdown next frame
    };

    void beginPanelHeader(std::string_view name, UIPanelState& state, const Rect& bounds, const std::vector<std::string_view>& tabs, bool showTabs);
    void drawPopup();
    // A row's icon at its left; returns the width it takes, gap included
    f32 drawIcon(const Rect& row, u32 icon);
    bool segmentedControl(std::string_view id, const Rect& rect, i32& index, const std::vector<std::string_view>& options);
    // dragFloat3 and dragFloat: count boxes side by side, each with its axis letter (none when the name is empty)
    bool dragFloats(std::string_view label, f32* const* components, const char* const* axisNames, const Color* const* axisColors,
                    u32 count, f32 speed, const char* format);
    void drawCheckbox(const Rect& box, bool value, const Interaction& interaction);

    Popup m_popup;
    bool m_popupOwnerSeen = false;
    i32 m_previousPopupRegion = -1;
    Rect m_viewport { 0.0f, 0.0f, 1.0e6f, 1.0e6f };

    std::unordered_map<UIId, ChildState> m_children;
    std::vector<ChildFrame> m_childStack;

    UIDrawList* m_drawList = nullptr;
    UIFont m_font;
};
