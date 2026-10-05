#include "ui/ui_context.hpp"
#include "ui/ui_style.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
    const Color& frameColor(bool hovered, bool held) {
        if (held) return UIStyle::FRAME_ACTIVE;
        if (hovered) return UIStyle::FRAME_HOVER;
        return UIStyle::FRAME;
    }

    Color toColor(const Vec3& rgb) {
        return { rgb.x, rgb.y, rgb.z, 1.0f };
    }
}

void UIContext::label(std::string_view text, bool dim) {
    drawLabelText(nextRow(), text, dim ? UIStyle::TEXT_DIM : UIStyle::TEXT);
}

void UIContext::heading(std::string_view text) {
    const Rect row = nextRow();
    drawLabelText(row, text, UIStyle::ACCENT);
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

bool UIContext::selectable(std::string_view label, bool selected) {
    const Rect row = nextRow();
    const Interaction interaction = interact(makeId(label), row);

    if (selected) {
        m_drawList->roundedRect(row, UIStyle::CORNER_RADIUS, UIStyle::ACCENT_SOFT);
        m_drawList->rect({ row.x, row.y + 3.0f, UIStyle::SELECTED_BAR_WIDTH, row.height - 6.0f }, UIStyle::ACCENT);
    } else if (interaction.hovered) {
        m_drawList->roundedRect(row, UIStyle::CORNER_RADIUS, UIStyle::ROW_HOVER);
    }

    drawLabelText({ row.x + UIStyle::TEXT_PADDING, row.y, row.width - UIStyle::TEXT_PADDING, row.height }, label, UIStyle::TEXT);

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

    drawLabelText(labelRect, label, UIStyle::TEXT_DIM);

    const f32 size = UIStyle::CHECKBOX_SIZE;
    const Rect box { controlRect.x, controlRect.y + std::floor((controlRect.height - size) * 0.5f), size, size };

    if (value) {
        m_drawList->roundedRect(box, 3.0f, UIStyle::ACCENT);
        m_drawList->line(Vec2(box.x + 3.5f, box.y + 7.5f), Vec2(box.x + 6.0f, box.y + 10.5f), 2.0f, UIStyle::TEXT_ON_ACCENT);
        m_drawList->line(Vec2(box.x + 6.0f, box.y + 10.5f), Vec2(box.x + 11.0f, box.y + 4.0f), 2.0f, UIStyle::TEXT_ON_ACCENT);
    } else {
        m_drawList->roundedRect(box, 3.0f, frameColor(interaction.hovered, interaction.held), UIStyle::FRAME_BORDER, 1.0f);
    }

    finishItem(interaction, changed);
    return changed;
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

    drawLabelText(labelRect, label, UIStyle::TEXT_DIM);

    Interaction interaction;
    const bool changed = sliderControl(makeId(label), controlRect, value, min, max, format, nullptr, interaction);

    finishItem(interaction, changed);
    return changed;
}

bool UIContext::dragFloat3(std::string_view label, Vec3& value, f32 speed, const char* format) {
    const Rect row = nextRow();
    Rect labelRect;
    Rect controlRect;
    splitLabeledRow(row, labelRect, controlRect);

    drawLabelText(labelRect, label, UIStyle::TEXT_DIM);

    const f32 componentWidth = std::floor((controlRect.width - UIStyle::COMPONENT_GAP * 2.0f) / 3.0f);
    f32* components[3] = { &value.x, &value.y, &value.z };
    const char* axisNames[3] = { "X", "Y", "Z" };
    const Color* axisColors[3] = { &UIStyle::AXIS_X, &UIStyle::AXIS_Y, &UIStyle::AXIS_Z };

    Interaction combined;
    bool changed = false;

    pushId(label);
    for (u32 i = 0; i < 3; ++i) {
        const Rect part { controlRect.x + i * (componentWidth + UIStyle::COMPONENT_GAP), controlRect.y, componentWidth, controlRect.height };
        const Interaction interaction = interact(makeId(axisNames[i]), part);

        // Dragging left/right nudges the value; it isn't tied to where in the box you click
        if (interaction.held && !interaction.activated && m_input.mouseDelta.x != 0.0f) {
            *components[i] += m_input.mouseDelta.x * speed;
            changed = true;
        }

        m_drawList->roundedRect(part, UIStyle::CORNER_RADIUS, frameColor(interaction.hovered, interaction.held));
        drawLabelText({ part.x + 6.0f, part.y, m_font.glyphWidth, part.height }, axisNames[i], *axisColors[i]);

        char text[32];
        std::snprintf(text, sizeof(text), format, *components[i]);
        const f32 textWidth = measureText(m_font, text).x;
        drawLabelText({ part.right() - 6.0f - textWidth, part.y, textWidth, part.height }, text, UIStyle::TEXT);

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

    drawLabelText(labelRect, label, UIStyle::TEXT_DIM);

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
