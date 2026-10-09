#include "ui/ui_context.hpp"
#include "ui/ui_style.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
    const Color& frameColor(bool hovered, bool held) {
        if (held) return UIStyle::FRAME_ACTIVE;
        if (hovered) return UIStyle::FRAME_HOVER;
        return UIStyle::FRAME;
    }

    Color toColor(const Vec3& rgb) {
        return { rgb.x, rgb.y, rgb.z, 1.0f };
    }

    constexpr f32 PICKER_HEIGHT = 120.0f;
    // Icons in rows: inset from the row's top and bottom, then a gap before the label
    constexpr f32 ICON_INSET = 2.0f;
    constexpr f32 ICON_GAP = 6.0f;
    constexpr f32 HUE_STRIP_WIDTH = 16.0f;

    // Under this much mouse movement, pressing and releasing a drag field is a click
    constexpr f32 DRAG_THRESHOLD = 3.0f;
    // Space between an X/Y/Z box's edge and its letter or value
    constexpr f32 COMPONENT_INSET = 4.0f;
    // Least space between a row's label and its control; a label that doesn't fit is shortened with "..."
    constexpr f32 LABEL_GAP = 4.0f;
    // Tree rows: how far each level is indented, and the fold arrow's column
    constexpr f32 TREE_INDENT = 14.0f;
    constexpr f32 TREE_ARROW_WIDTH = 16.0f;
    constexpr f32 TEXT_INSET = 6.0f;
    constexpr f64 CARET_BLINK = 0.5;   // seconds on, then off

    std::string trim(std::string_view text) {
        const auto begin = text.find_first_not_of(" \t");
        if (begin == std::string_view::npos) return {};
        const auto end = text.find_last_not_of(" \t");
        return std::string(text.substr(begin, end - begin + 1));
    }

    // Shortest text that reads back as the same value, for typing over
    std::string editableNumber(f32 value) {
        char text[32];
        std::snprintf(text, sizeof(text), "%g", static_cast<double>(value));
        return text;
    }

    bool parseNumber(const std::string& text, f32& value) {
        const std::string trimmed = trim(text);
        if (trimmed.empty()) return false;

        char* end = nullptr;
        const f32 parsed = std::strtof(trimmed.c_str(), &end);
        if (end != trimmed.c_str() + trimmed.size() || !std::isfinite(parsed)) return false;

        value = parsed;
        return true;
    }

    // value in format (like "%.2f"), or with fewer decimals until it fits in width
    std::string fitNumber(const UIFont& font, f32 value, const char* format, f32 width) {
        char text[32];
        std::snprintf(text, sizeof(text), format, static_cast<double>(value));
        // A value that rounds to zero shows as 0, not -0
        if (text[0] == '-' && std::strspn(text + 1, "0.") == std::strlen(text + 1)) std::memmove(text, text + 1, std::strlen(text));
        if (measureText(font, text).x <= width) return text;

        const char* dot = std::strchr(format, '.');
        const int decimals = dot ? std::atoi(dot + 1) : 0;
        for (int fewer = decimals - 1; fewer >= 0; --fewer) {
            std::snprintf(text, sizeof(text), "%.*f", fewer, static_cast<double>(value));
            if (measureText(font, text).x <= width) break;
        }
        return text;
    }

    // Clipboard text can hold anything; keep one line of characters the font can draw
    std::string printable(const std::string& text) {
        std::string result;
        for (char character : text) {
            if (character == '\n' || character == '\r') break;
            const unsigned char c = static_cast<unsigned char>(character);
            if (c >= 32 && c < 127) result += character;
        }
        return result;
    }
}

void UIContext::label(std::string_view text, bool dim) {
    const Rect row = nextRow();
    drawLabelText(row, fitText(m_font, text, row.width), dim ? UIStyle::TEXT_DIM : UIStyle::TEXT);
}

void UIContext::heading(std::string_view text) {
    const Rect row = nextRow();
    drawLabelText(row, text, UIStyle::ACCENT_GREEN);
}

bool UIContext::button(std::string_view label) {
    const Rect row = nextRow();
    const Interaction interaction = interact(makeId(label), row);

    m_drawList->roundedRect(row, UIStyle::CORNER_RADIUS, frameColor(interaction.hovered, interaction.held), UIStyle::FRAME_BORDER, 1.0f);

    const f32 textWidth = measureText(m_font, label).x;
    drawLabelText({ row.x + std::floor((row.width - textWidth) * 0.5f), row.y, textWidth, row.height }, label, UIStyle::TEXT);

    finishItem(interaction, false);
    return interaction.clicked;
}

bool UIContext::button(std::string_view label, const Rect& rect, bool enabled) {
    // A disabled button is drawn dim and ignores the mouse
    const Interaction interaction = enabled ? interact(makeId(label), rect) : Interaction {};

    m_drawList->roundedRect(rect, UIStyle::CORNER_RADIUS, enabled ? frameColor(interaction.hovered, interaction.held) : UIStyle::FRAME, UIStyle::FRAME_BORDER, 1.0f);

    const f32 textWidth = measureText(m_font, label).x;
    drawLabelText({ rect.x + std::floor((rect.width - textWidth) * 0.5f), rect.y, textWidth, rect.height }, label, enabled ? UIStyle::TEXT : UIStyle::TEXT_DIM);

    finishItem(interaction, false);
    return interaction.clicked;
}

bool UIContext::segmented(std::string_view label, i32& index, const std::vector<std::string_view>& options) {
    const Rect row = nextRow();
    Rect labelRect;
    Rect controlRect = row;
    if (!label.empty()) {
        splitLabeledRow(row, labelRect, controlRect);
        drawLabelText(labelRect, fitText(m_font, label, labelRect.width - LABEL_GAP), UIStyle::TEXT_DIM);
    }

    return segmentedControl(label.empty() ? std::string_view("segmented") : label, controlRect, index, options);
}

bool UIContext::segmented(std::string_view id, const Rect& rect, i32& index, const std::vector<std::string_view>& options) {
    return segmentedControl(id, rect, index, options);
}

bool UIContext::segmentedControl(std::string_view id, const Rect& controlRect, i32& index, const std::vector<std::string_view>& options) {
    m_drawList->roundedRect(controlRect, UIStyle::CORNER_RADIUS, UIStyle::FRAME, UIStyle::FRAME_BORDER, 1.0f);

    // Each segment fits its text, and leftover width is shared evenly
    const u32 count = static_cast<u32>(options.size());
    std::vector<f32> widths(count);
    f32 total = 0.0f;
    for (u32 i = 0; i < count; ++i) {
        widths[i] = measureText(m_font, options[i]).x + UIStyle::TEXT_PADDING * 2.0f;
        total += widths[i];
    }
    const f32 extra = (controlRect.width - total) / static_cast<f32>(count);
    for (f32& width : widths) width += extra;

    Interaction combined;
    bool changed = false;
    f32 x = controlRect.x;

    pushId(id);
    for (u32 i = 0; i < count; ++i) {
        const Rect segment { x, controlRect.y, widths[i], controlRect.height };
        x += widths[i];
        const Interaction interaction = interact(makeId(options[i]), segment);

        // Picks on release, like a button
        if (interaction.clicked && index != static_cast<i32>(i)) {
            index = static_cast<i32>(i);
            changed = true;
        }

        const bool selected = index == static_cast<i32>(i);
        if (selected) {
            m_drawList->roundedRect(segment, UIStyle::CORNER_RADIUS, UIStyle::ACCENT_FILL);
        } else if (interaction.hovered || interaction.held) {
            m_drawList->roundedRect(segment, UIStyle::CORNER_RADIUS, interaction.held ? UIStyle::FRAME_ACTIVE : UIStyle::FRAME_HOVER);
        }

        // Clipped so a label can never spill into its neighbor
        const f32 textWidth = measureText(m_font, options[i]).x;
        m_drawList->pushClip(segment);
        drawLabelText({ segment.x + std::floor((segment.width - textWidth) * 0.5f), segment.y, textWidth, segment.height }, options[i],
                      selected ? UIStyle::TEXT : UIStyle::TEXT_DIM);
        m_drawList->popClip();

        combined.hovered |= interaction.hovered;
        combined.activated |= interaction.activated;
        combined.held |= interaction.held;
        combined.deactivated |= interaction.deactivated;
    }
    popId();

    finishItem(combined, changed);
    return changed;
}

bool UIContext::dropdown(std::string_view label, const Rect& rect, i32& index, const std::vector<std::string_view>& options,
                         const std::vector<u32>& icons) {
    const UIId id = makeId(label);
    const bool ownsPopup = m_popup.open && m_popup.owner == id;
    if (ownsPopup) m_popupOwnerSeen = true;

    // A pick made in the list last frame lands here and closes it
    bool changed = false;
    if (ownsPopup && m_popup.picked >= 0) {
        if (index != m_popup.picked) {
            index = m_popup.picked;
            changed = true;
        }
        m_popup.open = false;
    }

    const Interaction interaction = interact(id, rect);
    if (interaction.clicked) {
        if (m_popup.open && m_popup.owner == id) {
            m_popup.open = false;
        } else {
            m_popup = { true, id, rect, std::vector<std::string>(options.begin(), options.end()), icons, index, -1 };
            m_popupOwnerSeen = true;
        }
    }

    const bool open = m_popup.open && m_popup.owner == id;
    m_drawList->roundedRect(rect, UIStyle::CORNER_RADIUS, frameColor(interaction.hovered || open, interaction.held), open ? UIStyle::ACCENT : UIStyle::FRAME_BORDER, 1.0f);

    const bool valid = index >= 0 && index < static_cast<i32>(options.size());
    const std::string_view text = valid ? options[index] : std::string_view();
    const u32 icon = valid && index < static_cast<i32>(icons.size()) ? icons[index] : 0;
    const f32 textLeft = rect.x + UIStyle::TEXT_PADDING + (icon != 0 ? drawIcon(rect, icon) : 0.0f);
    m_drawList->pushClip({ rect.x, rect.y, rect.width - 18.0f, rect.height });
    drawLabelText({ textLeft, rect.y, rect.right() - textLeft, rect.height }, text, UIStyle::TEXT);
    m_drawList->popClip();

    // Small chevron on the right
    const Vec2 tip(rect.right() - 11.0f, rect.y + rect.height * 0.5f + 2.0f);
    m_drawList->line(Vec2(tip.x - 4.0f, tip.y - 4.0f), tip, 1.5f, UIStyle::TEXT_DIM);
    m_drawList->line(tip, Vec2(tip.x + 4.0f, tip.y - 4.0f), 1.5f, UIStyle::TEXT_DIM);

    finishItem(interaction, changed);
    return changed;
}

f32 UIContext::drawIcon(const Rect& row, u32 icon) {
    const f32 size = row.height - ICON_INSET * 2.0f;
    m_drawList->image({ row.x + UIStyle::TEXT_PADDING, row.y + ICON_INSET, size, size }, icon);
    return size + ICON_GAP;
}

bool UIContext::selectable(std::string_view label, bool selected, std::string_view detail, u32 icon) {
    const Rect row = nextRow();
    const Interaction interaction = interact(makeId(label), row);

    if (selected) {
        m_drawList->roundedRect(row, UIStyle::CORNER_RADIUS, UIStyle::ACCENT_SOFT);
        m_drawList->rect({ row.x, row.y + 3.0f, UIStyle::SELECTED_BAR_WIDTH, row.height - 6.0f }, UIStyle::ACCENT);
    } else if (interaction.hovered) {
        m_drawList->roundedRect(row, UIStyle::CORNER_RADIUS, UIStyle::ROW_HOVER);
    }

    // The name gets whatever the detail leaves, and is shortened with "..." rather than running into it
    const f32 iconWidth = icon != 0 ? drawIcon(row, icon) : 0.0f;
    const f32 detailWidth = detail.empty() ? 0.0f : measureText(m_font, detail).x + UIStyle::TEXT_PADDING;
    const f32 labelRoom = row.width - UIStyle::TEXT_PADDING * 2.0f - detailWidth - iconWidth;
    drawLabelText({ row.x + UIStyle::TEXT_PADDING + iconWidth, row.y, labelRoom, row.height }, fitText(m_font, label, labelRoom), UIStyle::TEXT);

    if (!detail.empty()) {
        const f32 width = detailWidth - UIStyle::TEXT_PADDING;
        drawLabelText({ row.right() - UIStyle::TEXT_PADDING - width, row.y, width, row.height }, detail, UIStyle::TEXT_DIM);
    }

    finishItem(interaction, false);
    return interaction.clicked;
}

bool UIContext::treeRow(std::string_view label, bool selected, std::string_view detail, u32 depth, bool hasChildren, bool& open) {
    const Rect row = nextRow();
    const f32 indent = static_cast<f32>(depth) * TREE_INDENT;
    const Rect arrow { row.x + indent, row.y, TREE_ARROW_WIDTH, row.height };

    // The arrow is its own widget, checked first so a click on it folds rather than selects
    if (hasChildren) {
        pushId(label);
        const Interaction fold = interact(makeId("##fold"), arrow);
        popId();
        if (fold.clicked) open = !open;
    }

    const Interaction interaction = interact(makeId(label), row);

    if (selected) {
        m_drawList->roundedRect(row, UIStyle::CORNER_RADIUS, UIStyle::ACCENT_SOFT);
        m_drawList->rect({ row.x, row.y + 3.0f, UIStyle::SELECTED_BAR_WIDTH, row.height - 6.0f }, UIStyle::ACCENT);
    } else if (interaction.hovered) {
        m_drawList->roundedRect(row, UIStyle::CORNER_RADIUS, UIStyle::ROW_HOVER);
    }

    if (hasChildren) {
        // A small triangle: pointing right when folded, down when open
        const Vec2 c = arrow.center();
        const f32 s = 3.5f;
        if (open) {
            m_drawList->line(Vec2(c.x - s, c.y - s * 0.5f), Vec2(c.x, c.y + s * 0.5f), 1.5f, UIStyle::TEXT_DIM);
            m_drawList->line(Vec2(c.x, c.y + s * 0.5f), Vec2(c.x + s, c.y - s * 0.5f), 1.5f, UIStyle::TEXT_DIM);
        } else {
            m_drawList->line(Vec2(c.x - s * 0.5f, c.y - s), Vec2(c.x + s * 0.5f, c.y), 1.5f, UIStyle::TEXT_DIM);
            m_drawList->line(Vec2(c.x + s * 0.5f, c.y), Vec2(c.x - s * 0.5f, c.y + s), 1.5f, UIStyle::TEXT_DIM);
        }
    }

    const f32 textX = arrow.right();
    const f32 detailWidth = detail.empty() ? 0.0f : measureText(m_font, detail).x + UIStyle::TEXT_PADDING;
    const f32 labelRoom = row.right() - UIStyle::TEXT_PADDING - detailWidth - textX;
    drawLabelText({ textX, row.y, labelRoom, row.height }, fitText(m_font, label, labelRoom), UIStyle::TEXT);
    if (!detail.empty()) {
        const f32 width = detailWidth - UIStyle::TEXT_PADDING;
        drawLabelText({ row.right() - UIStyle::TEXT_PADDING - width, row.y, width, row.height }, detail, UIStyle::TEXT_DIM);
    }

    // Leave the row as the last item, for dragSource and lastItemRect
    m_lastItemId = makeId(label);
    m_lastRect = row;
    finishItem(interaction, false);
    return interaction.clicked;
}

bool UIContext::checkbox(std::string_view label, bool& value) {
    const Rect row = nextRow();
    Rect labelRect;
    Rect controlRect;
    splitLabeledRow(row, labelRect, controlRect);

    const Interaction interaction = interact(makeId(label), row);

    // Toggles on release, like a button
    const bool changed = interaction.clicked;
    if (changed) value = !value;

    drawLabelText(labelRect, fitText(m_font, label, labelRect.width - LABEL_GAP), UIStyle::TEXT_DIM);

    const f32 size = UIStyle::CHECKBOX_SIZE;
    drawCheckbox({ controlRect.x, controlRect.y + std::floor((controlRect.height - size) * 0.5f), size, size }, value, interaction);

    finishItem(interaction, changed);
    return changed;
}

bool UIContext::checkbox(std::string_view id, const Rect& rect, bool& value) {
    const Interaction interaction = interact(makeId(id), rect);
    const bool changed = interaction.clicked;
    if (changed) value = !value;

    const f32 size = UIStyle::CHECKBOX_SIZE;
    drawCheckbox({ rect.x, rect.y + std::floor((rect.height - size) * 0.5f), size, size }, value, interaction);

    finishItem(interaction, changed);
    return changed;
}

void UIContext::drawCheckbox(const Rect& box, bool value, const Interaction& interaction) {
    if (value) {
        m_drawList->roundedRect(box, 3.0f, UIStyle::ACCENT);
        m_drawList->line(Vec2(box.x + 3.5f, box.y + 7.5f), Vec2(box.x + 6.0f, box.y + 10.5f), 2.0f, UIStyle::TEXT_ON_ACCENT);
        m_drawList->line(Vec2(box.x + 6.0f, box.y + 10.5f), Vec2(box.x + 11.0f, box.y + 4.0f), 2.0f, UIStyle::TEXT_ON_ACCENT);
    } else {
        m_drawList->roundedRect(box, 3.0f, frameColor(interaction.hovered, interaction.held), UIStyle::FRAME_BORDER, 1.0f);
    }
}

bool UIContext::sliderControl(UIId id, const Rect& rect, f32& value, f32 min, f32 max, const char* format, const Color* fillColor, Interaction& interaction) {
    interaction = interact(id, rect);

    // The value follows the mouse while held and on release, but not on the press frame
    bool changed = false;
    if ((interaction.held || interaction.deactivated) && !interaction.activated && max > min) {
        const f32 t = std::clamp((m_input.mouse.x - rect.x) / rect.width, 0.0f, 1.0f);
        const f32 newValue = min + t * (max - min);
        if (newValue != value) {
            value = newValue;
            changed = true;
        }
    }

    m_drawList->roundedRect(rect, UIStyle::CORNER_RADIUS, frameColor(interaction.hovered, interaction.held));

    const f32 t = max > min ? std::clamp((value - min) / (max - min), 0.0f, 1.0f) : 0.0f;
    if (t > 0.0f) {
        m_drawList->pushClip({ rect.x, rect.y, rect.width * t, rect.height });
        m_drawList->roundedRect(rect, UIStyle::CORNER_RADIUS, fillColor ? *fillColor : UIStyle::ACCENT_FILL);
        m_drawList->popClip();
    }

    char text[32];
    std::snprintf(text, sizeof(text), format, value);
    const f32 textWidth = measureText(m_font, text).x;
    drawLabelText({ rect.x + std::floor((rect.width - textWidth) * 0.5f), rect.y, textWidth, rect.height }, text, UIStyle::TEXT);

    return changed;
}

bool UIContext::sliderFloat(std::string_view label, f32& value, f32 min, f32 max, const char* format) {
    const Rect row = nextRow();
    Rect labelRect;
    Rect controlRect;
    splitLabeledRow(row, labelRect, controlRect);

    drawLabelText(labelRect, fitText(m_font, label, labelRect.width - LABEL_GAP), UIStyle::TEXT_DIM);

    Interaction interaction;
    const bool changed = sliderControl(makeId(label), controlRect, value, min, max, format, nullptr, interaction);

    finishItem(interaction, changed);
    return changed;
}

bool UIContext::dragFloat3(std::string_view label, Vec3& value, f32 speed, const char* format) {
    f32* components[3] = { &value.x, &value.y, &value.z };
    const char* axisNames[3] = { "X", "Y", "Z" };
    const Color* axisColors[3] = { &UIStyle::AXIS_X, &UIStyle::AXIS_Y, &UIStyle::AXIS_Z };
    return dragFloats(label, components, axisNames, axisColors, 3, speed, format);
}

bool UIContext::dragFloat(std::string_view label, f32& value, f32 speed, const char* format) {
    f32* components[1] = { &value };
    const char* names[1] = { "" };
    const Color* colors[1] = { &UIStyle::TEXT_DIM };
    return dragFloats(label, components, names, colors, 1, speed, format);
}

bool UIContext::dragFloats(std::string_view label, f32* const* components, const char* const* axisNames, const Color* const* axisColors,
                           u32 count, f32 speed, const char* format) {
    const Rect row = nextRow();
    Rect labelRect;
    Rect controlRect;
    splitLabeledRow(row, labelRect, controlRect);

    drawLabelText(labelRect, fitText(m_font, label, labelRect.width - LABEL_GAP), UIStyle::TEXT_DIM);

    const f32 componentWidth = std::floor((controlRect.width - UIStyle::COMPONENT_GAP * (count - 1)) / count);

    Interaction combined;
    bool changed = false;

    pushId(label);
    for (u32 i = 0; i < count; ++i) {
        const Rect part { controlRect.x + i * (componentWidth + UIStyle::COMPONENT_GAP), controlRect.y, componentWidth, controlRect.height };
        // A single box has no axis letter; its id still needs a name
        const bool lettered = axisNames[i][0] != '\0';
        const UIId id = makeId(lettered ? axisNames[i] : "value");

        // Being typed into: one undo step when the typed value is kept
        if (m_textEdit.id == id) {
            std::string typed;
            if (textEditBox(part, typed) == TextEditResult::Committed) {
                f32 parsed = 0.0f;
                if (parseNumber(typed, parsed) && parsed != *components[i]) {
                    *components[i] = parsed;
                    changed = true;
                }
                combined.activated = true;
                combined.deactivated = true;
            }
            continue;
        }

        const Interaction interaction = interact(id, part);

        // Dragging left/right nudges the value once the mouse has moved past the threshold; it isn't tied to where in the box you click
        if (interaction.held && !interaction.activated && m_activeTravel >= DRAG_THRESHOLD && m_input.mouseDelta.x != 0.0f) {
            *components[i] += m_input.mouseDelta.x * speed;
            changed = true;
        }

        // A click without a drag opens the value for typing
        if (interaction.deactivated && interaction.clicked && m_activeTravel < DRAG_THRESHOLD) {
            beginTextEdit(id, editableNumber(*components[i]));
        }

        m_drawList->roundedRect(part, UIStyle::CORNER_RADIUS, frameColor(interaction.hovered, interaction.held));
        if (lettered) drawLabelText({ part.x + COMPONENT_INSET, part.y, m_font.glyphWidth, part.height }, axisNames[i], *axisColors[i]);

        // The value sits right of the axis letter; when it doesn't fit (a narrow panel, a minus sign), fewer decimals
        // are shown, and anything still too long is clipped rather than drawn over the letter
        const f32 valueLeft = part.x + COMPONENT_INSET + (lettered ? m_font.glyphWidth + 2.0f : 0.0f);
        const f32 room = part.right() - COMPONENT_INSET - valueLeft;
        const std::string text = fitNumber(m_font, *components[i], format, room);
        const f32 textWidth = std::min(measureText(m_font, text).x, room);
        m_drawList->pushClip({ valueLeft, part.y, std::max(0.0f, room), part.height });
        drawLabelText({ part.right() - COMPONENT_INSET - textWidth, part.y, textWidth, part.height }, text, UIStyle::TEXT);
        m_drawList->popClip();

        combined.hovered |= interaction.hovered;
        combined.activated |= interaction.activated;
        combined.held |= interaction.held;
        combined.deactivated |= interaction.deactivated;
    }
    popId();

    finishItem(combined, changed);
    return changed;
}

bool UIContext::colorEdit(std::string_view label, Vec3& color) {
    const UIId id = makeId(label);
    const Rect row = nextRow();
    Rect labelRect;
    Rect controlRect;
    splitLabeledRow(row, labelRect, controlRect);

    drawLabelText(labelRect, fitText(m_font, label, labelRect.width - LABEL_GAP), UIStyle::TEXT_DIM);

    // Clicking the swatch opens or closes the picker under it; it remembers the color it opened with
    const Interaction swatch = interact(id, controlRect);
    if (swatch.clicked) {
        if (m_colorPickers.count(id)) {
            m_colorPickers.erase(id);
        } else {
            ColorPicker picker;
            picker.opened = color;
            rgbToHsv(color, picker.hue, picker.saturation, picker.value);
            m_colorPickers[id] = picker;
        }
    }

    const auto found = m_colorPickers.find(id);
    const bool open = found != m_colorPickers.end();
    const Color border = swatch.hovered || open ? UIStyle::ACCENT : UIStyle::FRAME_BORDER;

    // While open, the left half shows the color it had when opened, so a change can be compared
    const bool split = open && !(found->second.opened.x == color.x && found->second.opened.y == color.y && found->second.opened.z == color.z);
    if (split) {
        const f32 half = std::floor(controlRect.width * 0.5f);
        m_drawList->roundedRect(controlRect, UIStyle::CORNER_RADIUS, toColor(color), border, 1.0f);
        m_drawList->pushClip({ controlRect.x, controlRect.y, half, controlRect.height });
        m_drawList->roundedRect(controlRect, UIStyle::CORNER_RADIUS, toColor(found->second.opened), border, 1.0f);
        m_drawList->popClip();
    } else {
        m_drawList->roundedRect(controlRect, UIStyle::CORNER_RADIUS, toColor(color), border, 1.0f);
    }

    Interaction combined = swatch;
    bool changed = false;

    // The parts below report their own interactions; they're folded into this one item, so a whole drag or one
    // typed value is a single edit
    const auto absorb = [&](bool partChanged) {
        changed |= partChanged;
        combined.activated |= m_last.activated;
        combined.deactivated |= m_last.deactivated;
        combined.hovered |= m_last.hovered;
    };

    if (open) {
        ColorPicker& picker = found->second;

        // Typed or picked elsewhere since last frame (undo, another field): follow it, keeping the hue of grays
        f32 hue = 0.0f, saturation = 0.0f, value = 0.0f;
        rgbToHsv(color, hue, saturation, value);
        if (!(std::abs(hsvToRgb(picker.hue, picker.saturation, picker.value).x - color.x) < 1e-4f
              && std::abs(hsvToRgb(picker.hue, picker.saturation, picker.value).y - color.y) < 1e-4f
              && std::abs(hsvToRgb(picker.hue, picker.saturation, picker.value).z - color.z) < 1e-4f)) {
            if (saturation > 0.0f && value > 0.0f) picker.hue = hue;
            if (value > 0.0f) picker.saturation = saturation;
            picker.value = value;
        }

        pushId(label);

        // Saturation (left to right) and value (top to bottom) square, with the hue strip to its right
        const Rect area = nextRow(PICKER_HEIGHT);
        const Rect square { controlRect.x, area.y, controlRect.width - HUE_STRIP_WIDTH - UIStyle::COMPONENT_GAP, area.height };
        const Rect strip { square.right() + UIStyle::COMPONENT_GAP, area.y, HUE_STRIP_WIDTH, area.height };

        const Interaction squareInteraction = interact(makeId("square"), square);
        if (squareInteraction.held) {
            picker.saturation = std::clamp((m_input.mouse.x - square.x) / square.width, 0.0f, 1.0f);
            picker.value = 1.0f - std::clamp((m_input.mouse.y - square.y) / square.height, 0.0f, 1.0f);
        }
        const Interaction stripInteraction = interact(makeId("hue"), strip);
        if (stripInteraction.held) picker.hue = std::clamp((m_input.mouse.y - strip.y) / strip.height, 0.0f, 1.0f);

        if (squareInteraction.held || stripInteraction.held) {
            const Vec3 picked = hsvToRgb(picker.hue, picker.saturation, picker.value);
            if (picked.x != color.x || picked.y != color.y || picked.z != color.z) {
                color = picked;
                changed = true;
            }
        }
        for (const Interaction& part : { squareInteraction, stripInteraction }) {
            combined.activated |= part.activated;
            combined.held |= part.held;
            combined.deactivated |= part.deactivated;
        }

        const Vec3 pure = hsvToRgb(picker.hue, 1.0f, 1.0f);
        m_drawList->gradientRect(square, { 1, 1, 1, 1 }, toColor(pure), toColor(pure), { 1, 1, 1, 1 });
        m_drawList->gradientRect(square, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 1 }, { 0, 0, 0, 1 });
        m_drawList->roundedRect(square, 0.0f, {}, UIStyle::FRAME_BORDER, 1.0f);

        // Six bands, red to red
        for (u32 band = 0; band < 6; ++band) {
            const f32 top = strip.y + strip.height * band / 6.0f;
            const f32 bottom = strip.y + strip.height * (band + 1) / 6.0f;
            const Color from = toColor(hsvToRgb(band / 6.0f, 1.0f, 1.0f));
            const Color to = toColor(hsvToRgb((band + 1) / 6.0f, 1.0f, 1.0f));
            m_drawList->gradientRect({ strip.x, top, strip.width, bottom - top }, from, from, to, to);
        }
        m_drawList->roundedRect(strip, 0.0f, {}, UIStyle::FRAME_BORDER, 1.0f);

        // Markers: a ring at the picked spot, a bar at the hue
        const Vec2 spot(square.x + picker.saturation * square.width, square.y + (1.0f - picker.value) * square.height);
        m_drawList->roundedRect({ spot.x - 5.0f, spot.y - 5.0f, 10.0f, 10.0f }, 5.0f, {}, { 0, 0, 0, 0.8f }, 3.0f);
        m_drawList->roundedRect({ spot.x - 4.0f, spot.y - 4.0f, 8.0f, 8.0f }, 4.0f, {}, { 1, 1, 1, 1 }, 1.5f);
        const f32 hueY = strip.y + picker.hue * strip.height;
        m_drawList->roundedRect({ strip.x - 2.0f, hueY - 2.0f, strip.width + 4.0f, 4.0f }, 1.0f, { 1, 1, 1, 1 }, { 0, 0, 0, 0.8f }, 1.0f);

        // Hex, as image editors and web colors write it
        std::string hex = toHex(color);
        Vec3 parsed;
        if (textField("Hex", hex) && parseHex(hex, parsed)) {
            color = parsed;
            absorb(true);
        } else {
            absorb(false);
        }

        // Exact values
        f32* channels[3] = { &color.x, &color.y, &color.z };
        const char* channelNames[3] = { "R", "G", "B" };
        const Color* channelColors[3] = { &UIStyle::AXIS_X, &UIStyle::AXIS_Y, &UIStyle::AXIS_Z };
        const bool typed = dragFloats("RGB", channels, channelNames, channelColors, 3, 0.005f, "%.2f");
        color = Vec3(std::clamp(color.x, 0.0f, 1.0f), std::clamp(color.y, 0.0f, 1.0f), std::clamp(color.z, 0.0f, 1.0f));
        absorb(typed);

        popId();
    }

    // The swatch itself never edits, so only the parts count as the start and end of an edit
    combined.activated &= !swatch.activated;
    combined.deactivated &= !swatch.deactivated;

    finishItem(combined, changed);
    return changed;
}

bool UIContext::textField(std::string_view label, std::string& text, bool allowEmpty) {
    const UIId id = makeId(label);
    const Rect row = nextRow();
    Rect labelRect;
    Rect controlRect;
    splitLabeledRow(row, labelRect, controlRect);

    drawLabelText(labelRect, fitText(m_font, label, labelRect.width - LABEL_GAP), UIStyle::TEXT_DIM);

    Interaction interaction;
    bool changed = false;

    if (m_textEdit.id == id) {
        std::string typed;
        if (textEditBox(controlRect, typed) == TextEditResult::Committed) {
            const std::string kept = trim(typed);
            if ((allowEmpty || !kept.empty()) && kept != text) {
                text = kept;
                changed = true;
            }

            // The whole edit counts as one press-to-release interaction, so it's one undo step
            interaction.activated = true;
            interaction.deactivated = true;
        }
    } else {
        const Interaction pressed = interact(id, controlRect);
        interaction.hovered = pressed.hovered;
        if (pressed.hovered) m_cursor = UICursor::Text;

        m_drawList->roundedRect(controlRect, UIStyle::CORNER_RADIUS, frameColor(pressed.hovered, false), UIStyle::FRAME_BORDER, 1.0f);

        const Rect inner { controlRect.x + TEXT_INSET, controlRect.y, std::max(0.0f, controlRect.width - TEXT_INSET * 2.0f), controlRect.height };
        m_drawList->pushClip(inner);
        drawLabelText(inner, text, UIStyle::TEXT);
        m_drawList->popClip();

        // Editing starts on the press, with everything selected so typing replaces it
        if (pressed.activated) beginTextEdit(id, text);
    }

    finishItem(interaction, changed);
    return changed;
}

void UIContext::beginTextEdit(UIId id, const std::string& text) {
    m_textEdit = {};
    m_textEdit.id = id;
    m_textEdit.buffer = text;
    m_textEdit.anchor = 0;
    m_textEdit.caret = static_cast<u32>(text.size());
    m_textEdit.lastInput = m_input.time;
    m_textEdit.seen = true;
    m_textEdit.justStarted = true;
}

UIContext::TextEditResult UIContext::textEditBox(const Rect& rect, std::string& committed) {
    TextEdit& edit = m_textEdit;
    edit.seen = true;
    edit.rect = rect;

    std::string& buffer = edit.buffer;
    const f32 glyph = m_font.glyphWidth;
    const Rect inner { rect.x + TEXT_INSET, rect.y, std::max(0.0f, rect.width - TEXT_INSET * 2.0f), rect.height };

    auto finish = [&](TextEditResult result) {
        if (result == TextEditResult::Committed) committed = buffer;
        m_textEdit = {};
        return result;
    };

    auto selectionStart = [&] { return std::min(edit.caret, edit.anchor); };
    auto selectionEnd = [&] { return std::max(edit.caret, edit.anchor); };

    auto eraseSelection = [&] {
        const u32 start = selectionStart();
        buffer.erase(start, selectionEnd() - start);
        edit.caret = edit.anchor = start;
    };

    auto insert = [&](const std::string& text) {
        if (edit.caret != edit.anchor) eraseSelection();
        buffer.insert(edit.caret, text);
        edit.caret += static_cast<u32>(text.size());
        edit.anchor = edit.caret;
    };

    auto caretAtMouse = [&] {
        const f32 offset = (m_input.mouse.x - inner.x + edit.scroll) / glyph;
        return static_cast<u32>(std::clamp(std::lround(offset), 0l, static_cast<long>(buffer.size())));
    };

    // 1. Mouse: a press anywhere else keeps the edit; inside, it places the caret and drags a selection
    const bool hovered = m_interactive && rect.contains(m_input.mouse);
    if (hovered) m_cursor = UICursor::Text;

    if (!edit.justStarted) {
        if (m_input.anyPressed() && !rect.contains(m_input.mouse)) return finish(TextEditResult::Committed);

        if (m_input.pressed[UIInput::LEFT] && hovered) {
            edit.caret = caretAtMouse();
            if (!m_input.shift) edit.anchor = edit.caret;
            edit.dragging = true;
        }
    }

    if (edit.dragging && m_input.down[UIInput::LEFT] && !m_input.pressed[UIInput::LEFT]) edit.caret = caretAtMouse();
    if (!m_input.down[UIInput::LEFT]) edit.dragging = false;
    edit.justStarted = false;

    // 2. Keys, in the order they were pressed
    for (UIKey key : m_input.keys) {
        const bool selected = edit.caret != edit.anchor;
        edit.lastInput = m_input.time;

        switch (key) {
            case UIKey::Left:
                if (selected && !m_input.shift) edit.caret = selectionStart();
                else if (edit.caret > 0) --edit.caret;
                if (!m_input.shift) edit.anchor = edit.caret;
                break;
            case UIKey::Right:
                if (selected && !m_input.shift) edit.caret = selectionEnd();
                else if (edit.caret < buffer.size()) ++edit.caret;
                if (!m_input.shift) edit.anchor = edit.caret;
                break;
            case UIKey::Home:
                edit.caret = 0;
                if (!m_input.shift) edit.anchor = edit.caret;
                break;
            case UIKey::End:
                edit.caret = static_cast<u32>(buffer.size());
                if (!m_input.shift) edit.anchor = edit.caret;
                break;
            case UIKey::Backspace:
                if (selected) {
                    eraseSelection();
                } else if (edit.caret > 0) {
                    buffer.erase(--edit.caret, 1);
                    edit.anchor = edit.caret;
                }
                break;
            case UIKey::Delete:
                if (selected) eraseSelection();
                else if (edit.caret < buffer.size()) buffer.erase(edit.caret, 1);
                break;
            case UIKey::Enter:
                return finish(TextEditResult::Committed);
            case UIKey::Escape:
                return finish(TextEditResult::Cancelled);
            case UIKey::SelectAll:
                edit.anchor = 0;
                edit.caret = static_cast<u32>(buffer.size());
                break;
            case UIKey::Copy:
            case UIKey::Cut:
                if (selected && m_setClipboard) m_setClipboard(buffer.substr(selectionStart(), selectionEnd() - selectionStart()));
                if (selected && key == UIKey::Cut) eraseSelection();
                break;
            case UIKey::Paste:
                if (m_getClipboard) insert(printable(m_getClipboard()));
                break;
        }
    }

    if (!m_input.text.empty()) {
        insert(m_input.text);
        edit.lastInput = m_input.time;
    }

    // 3. Scroll sideways so the caret stays in view
    const f32 caretX = static_cast<f32>(edit.caret) * glyph;
    if (caretX - edit.scroll > inner.width) edit.scroll = caretX - inner.width;
    if (caretX - edit.scroll < 0.0f) edit.scroll = caretX;
    edit.scroll = std::clamp(edit.scroll, 0.0f, std::max(0.0f, static_cast<f32>(buffer.size()) * glyph - inner.width + 1.0f));

    // 4. Draw: field, selection, text, caret
    m_drawList->roundedRect(rect, UIStyle::CORNER_RADIUS, UIStyle::FRAME_ACTIVE, UIStyle::ACCENT_GREEN, 1.0f);
    m_drawList->pushClip(inner);

    const f32 textY = std::round(inner.y + (inner.height - m_font.glyphHeight) * 0.5f);
    const f32 originX = inner.x - edit.scroll;

    if (edit.caret != edit.anchor) {
        const f32 left = originX + static_cast<f32>(selectionStart()) * glyph;
        const f32 width = static_cast<f32>(selectionEnd() - selectionStart()) * glyph;
        m_drawList->rect({ left, textY, width, m_font.glyphHeight }, UIStyle::ACCENT_FILL);
    }

    m_drawList->text(Vec2(originX, textY), buffer, m_font, UIStyle::TEXT);

    const bool caretOn = std::fmod(m_input.time - edit.lastInput, CARET_BLINK * 2.0) < CARET_BLINK;
    if (caretOn) m_drawList->rect({ std::round(originX + caretX), textY, 1.0f, m_font.glyphHeight }, UIStyle::ACCENT_GREEN);

    m_drawList->popClip();
    return TextEditResult::Editing;
}

void rgbToHsv(const Vec3& rgb, f32& hue, f32& saturation, f32& value) {
    const f32 high = std::max(rgb.x, std::max(rgb.y, rgb.z));
    const f32 low = std::min(rgb.x, std::min(rgb.y, rgb.z));
    const f32 range = high - low;

    value = high;
    saturation = high > 0.0f ? range / high : 0.0f;

    if (range <= 0.0f) {
        hue = 0.0f;
        return;
    }

    f32 sixths = 0.0f;
    if (high == rgb.x) sixths = std::fmod((rgb.y - rgb.z) / range + 6.0f, 6.0f);
    else if (high == rgb.y) sixths = (rgb.z - rgb.x) / range + 2.0f;
    else sixths = (rgb.x - rgb.y) / range + 4.0f;
    hue = sixths / 6.0f;
}

Vec3 hsvToRgb(f32 hue, f32 saturation, f32 value) {
    const f32 sixths = std::fmod(std::clamp(hue, 0.0f, 1.0f) * 6.0f, 6.0f);
    const i32 sector = static_cast<i32>(sixths);
    const f32 fraction = sixths - sector;

    const f32 p = value * (1.0f - saturation);
    const f32 q = value * (1.0f - saturation * fraction);
    const f32 t = value * (1.0f - saturation * (1.0f - fraction));

    switch (sector) {
        case 0: return Vec3(value, t, p);
        case 1: return Vec3(q, value, p);
        case 2: return Vec3(p, value, t);
        case 3: return Vec3(p, q, value);
        case 4: return Vec3(t, p, value);
        default: return Vec3(value, p, q);
    }
}

std::string toHex(const Vec3& rgb) {
    char text[8];
    const auto byte = [](f32 channel) { return static_cast<int>(std::lround(std::clamp(channel, 0.0f, 1.0f) * 255.0f)); };
    std::snprintf(text, sizeof(text), "#%02x%02x%02x", byte(rgb.x), byte(rgb.y), byte(rgb.z));
    return text;
}

bool parseHex(std::string_view text, Vec3& out) {
    if (!text.empty() && text.front() == '#') text.remove_prefix(1);
    if (text.size() != 6 && text.size() != 3) return false;

    const auto digit = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };

    int channels[3];
    for (int i = 0; i < 3; ++i) {
        // #rgb doubles each digit: #f80 is #ff8800
        const int high = digit(text.size() == 6 ? text[i * 2] : text[i]);
        const int low = digit(text.size() == 6 ? text[i * 2 + 1] : text[i]);
        if (high < 0 || low < 0) return false;
        channels[i] = high * 16 + low;
    }

    out = Vec3(channels[0] / 255.0f, channels[1] / 255.0f, channels[2] / 255.0f);
    return true;
}
