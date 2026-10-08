#include "application/modal_windows.hpp"
#include "application/asset_actions.hpp"
#include "ui/ui_style.hpp"

#include <algorithm>
#include <cmath>

namespace {
    constexpr f32 PROMPT_MIN_WIDTH = 360.0f;
    constexpr f32 BUTTON_MIN_WIDTH = 96.0f;
    constexpr f32 BUTTON_GAP = 8.0f;

    f32 buttonWidth(const UIFont& font, const std::string& label) {
        return std::max(BUTTON_MIN_WIDTH, measureText(font, label).x + UIStyle::TEXT_PADDING * 4.0f);
    }

    void drawPrompt(AppContext& ctx, const Rect& viewport) {
        UIContext& ui = ctx.ui;
        PromptState& prompt = ctx.modal.prompt;
        const UIFont& font = ui.font();

        f32 buttonsWidth = 0.0f;
        for (const std::string& label : prompt.buttons) buttonsWidth += buttonWidth(font, label) + BUTTON_GAP;

        const f32 contentWidth = std::max(measureText(font, prompt.message).x, buttonsWidth);
        const f32 width = std::max(PROMPT_MIN_WIDTH, contentWidth + UIStyle::PADDING * 2.0f);
        const f32 height = UIStyle::PANEL_HEADER_HEIGHT + UIStyle::PADDING * 2.0f + UIStyle::ROW_HEIGHT * 2.0f + UIStyle::ITEM_SPACING + UIStyle::SECTION_SPACING;

        ui.beginModal("prompt", viewport, width, height, prompt.title);
        ui.label(prompt.message);
        ui.spacing();

        // Buttons on the right, in order; the default one is outlined in the accent color
        const Rect row = ui.row();
        f32 x = row.right();
        for (i32 i = static_cast<i32>(prompt.buttons.size()) - 1; i >= 0; --i) {
            const f32 w = buttonWidth(font, prompt.buttons[i]);
            x -= w;
            const Rect rect { x, row.y, w, row.height };
            if (ui.button(prompt.buttons[i], rect)) prompt.chosen = i;
            if (i == prompt.confirmButton) ui.drawList().roundedRect(rect, UIStyle::CORNER_RADIUS, { 0.0f, 0.0f, 0.0f, 0.0f }, UIStyle::ACCENT, 1.0f);
            x -= BUTTON_GAP;
        }

        ui.endModal();
    }
}

bool isModalOpen(const AppContext& ctx) {
    return ctx.modal.kind != ModalKind::None;
}

void openModal(AppContext& ctx, ModalKind kind) {
    ctx.modal.kind = kind;
    ctx.systems.input_ctx.addContext(InputContext_Modal);
    ctx.radialMenu.open = false;
}

void showPrompt(AppContext& ctx, std::string title, std::string message, std::vector<std::string> buttons,
                i32 confirmButton, i32 cancelButton, std::function<void(i32 button)> onChoice) {
    PromptState& prompt = ctx.modal.prompt;
    prompt.title = std::move(title);
    prompt.message = std::move(message);
    prompt.buttons = std::move(buttons);
    prompt.confirmButton = confirmButton;
    prompt.cancelButton = cancelButton;
    prompt.onChoice = std::move(onChoice);
    prompt.chosen = -1;
    openModal(ctx, ModalKind::Prompt);
}

void closeModal(AppContext& ctx) {
    ctx.modal.kind = ModalKind::None;
    ctx.systems.input_ctx.removeContext(InputContext_Modal);
}

void confirmModal(AppContext& ctx) {
    if (ctx.modal.kind == ModalKind::Prompt) ctx.modal.prompt.chosen = ctx.modal.prompt.confirmButton;
    else if (ctx.modal.kind == ModalKind::Export) confirmExportWindow(ctx);
}

void cancelModal(AppContext& ctx) {
    if (ctx.modal.kind == ModalKind::Prompt) {
        if (ctx.modal.prompt.cancelButton >= 0) ctx.modal.prompt.chosen = ctx.modal.prompt.cancelButton;
    } else if (ctx.modal.kind == ModalKind::Export) {
        closeModal(ctx);
    }
}

void drawModal(AppContext& ctx, const Rect& viewport) {
    if (ctx.modal.kind == ModalKind::Prompt) drawPrompt(ctx, viewport);
    else if (ctx.modal.kind == ModalKind::Export) drawExportWindow(ctx, viewport);
}

void updateModal(AppContext& ctx) {
    if (ctx.modal.kind == ModalKind::Export) {
        updateExportWindow(ctx);
        return;
    }

    PromptState& prompt = ctx.modal.prompt;
    if (ctx.modal.kind != ModalKind::Prompt || prompt.chosen < 0) return;

    // Close first, so the answer can open another prompt or a native dialog
    const i32 chosen = prompt.chosen;
    std::function<void(i32)> onChoice = std::move(prompt.onChoice);
    prompt = {};
    closeModal(ctx);
    if (onChoice) onChoice(chosen);
}
