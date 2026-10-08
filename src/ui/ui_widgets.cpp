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

bool UIContext::dropdown(std::string_view label, const Rect& rect, i32& index, const std::vector<std::string_view>& options) {
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
            m_popup = { true, id, rect, std::vector<std::string>(options.begin(), options.end()), index, -1 };
            m_popupOwnerSeen = true;
        }
    }

    const bool open = m_popup.open && m_popup.owner == id;
    m_drawList->roundedRect(rect, UIStyle::CORNER_RADIUS, frameColor(interaction.hovered || open, interaction.held), open ? UIStyle::ACCENT : UIStyle::FRAME_BORDER, 1.0f);

    const std::string_view text = index >= 0 && index < static_cast<i32>(options.size()) ? options[index] : std::string_view();
    m_drawList->pushClip({ rect.x, rect.y, rect.width - 18.0f, rect.height });
    drawLabelText({ rect.x + UIStyle::TEXT_PADDING, rect.y, rect.width, rect.height }, text, UIStyle::TEXT);
    m_drawList->popClip();

    // Small chevron on the right
    const Vec2 tip(rect.right() - 11.0f, rect.y + rect.height * 0.5f + 2.0f);
    m_drawList->line(Vec2(tip.x - 4.0f, tip.y - 4.0f), tip, 1.5f, UIStyle::TEXT_DIM);
    m_drawList->line(tip, Vec2(tip.x + 4.0f, tip.y - 4.0f), 1.5f, UIStyle::TEXT_DIM);

    finishItem(interaction, changed);
    return changed;
}

bool UIContext::selectable(std::string_view label, bool selected, std::string_view detail) {
    const Rect row = nextRow();
    const Interaction interaction = interact(makeId(label), row);

    if (selected) {
        m_drawList->roundedRect(row, UIStyle::CORNER_RADIUS, UIStyle::ACCENT_SOFT);
        m_drawList->rect({ row.x, row.y + 3.0f, UIStyle::SELECTED_BAR_WIDTH, row.height - 6.0f }, UIStyle::ACCENT);
    } else if (interaction.hovered) {
        m_drawList->roundedRect(row, UIStyle::CORNER_RADIUS, UIStyle::ROW_HOVER);
    }

    // The name gets whatever the detail leaves, and is shortened with "..." rather than running into it
    const f32 detailWidth = detail.empty() ? 0.0f : measureText(m_font, detail).x + UIStyle::TEXT_PADDING;
    const f32 labelRoom = row.width - UIStyle::TEXT_PADDING * 2.0f - detailWidth;
    drawLabelText({ row.x + UIStyle::TEXT_PADDING, row.y, labelRoom, row.height }, fitText(m_font, label, labelRoom), UIStyle::TEXT);

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

    // Clicking the swatch shows or hides the RGB sliders under it
    const Interaction swatch = interact(id, controlRect);
    if (swatch.clicked) {
        if (m_openWidgets.count(id)) m_openWidgets.erase(id);
        else m_openWidgets.insert(id);
    }

    const bool open = m_openWidgets.count(id) > 0;
    m_drawList->roundedRect(controlRect, UIStyle::CORNER_RADIUS, toColor(color), swatch.hovered || open ? UIStyle::ACCENT : UIStyle::FRAME_BORDER, 1.0f);

    Interaction combined = swatch;
    bool changed = false;

    if (open) {
        const Color channelColors[3] = { UIStyle::AXIS_X, UIStyle::AXIS_Y, UIStyle::AXIS_Z };
        const char* channelNames[3] = { "R", "G", "B" };
        f32* channels[3] = { &color.x, &color.y, &color.z };

        pushId(label);
        for (u32 i = 0; i < 3; ++i) {
            const Rect channelRow = nextRow();
            Rect channelLabel;
            Rect channelControl;
            splitLabeledRow(channelRow, channelLabel, channelControl);

            drawLabelText({ channelControl.x - m_font.glyphWidth - 6.0f, channelRow.y, m_font.glyphWidth, channelRow.height }, channelNames[i], channelColors[i]);

            Color fill = channelColors[i];
            fill.a = 0.45f;

            Interaction interaction;
            changed |= sliderControl(makeId(channelNames[i]), channelControl, *channels[i], 0.0f, 1.0f, "%.2f", &fill, interaction);

            combined.activated |= interaction.activated;
            combined.held |= interaction.held;
            combined.deactivated |= interaction.deactivated;
        }
        popId();
    }

    // The swatch itself never edits, so only the sliders count as the start and end of an edit
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
