#pragma once

#include <functional>
#include <string>
#include <vector>
#include "application/app_context.hpp"

// Modal windows drawn in the app's own UI: the prompt (a question with buttons) and the Export window.
// While one is open it takes all input (InputContext_Modal): only Enter and Escape work, and the backdrop
// blocks the panel and the viewport. Opening one closes whichever was open.

bool isModalOpen(const AppContext& ctx);

// Shows a modal window whose state is already filled in (showPrompt does this for prompts)
void openModal(AppContext& ctx, ModalKind kind);

// confirmButton answers Enter (and is drawn as the default), cancelButton answers Escape (-1 for none).
// onChoice runs in checkActions on the next frame, after the prompt has closed, so it can open another.
void showPrompt(AppContext& ctx, std::string title, std::string message, std::vector<std::string> buttons,
                i32 confirmButton, i32 cancelButton, std::function<void(i32 button)> onChoice);

void closeModal(AppContext& ctx);

// Enter and Escape (the ModalConfirm and ModalCancel actions)
void confirmModal(AppContext& ctx);
void cancelModal(AppContext& ctx);

// Draws the open modal window over everything; call inside the UI's draw, after every panel
void drawModal(AppContext& ctx, const Rect& viewport);

// Acts on what the last draw asked for (a clicked button, Browse, Export); call from checkActions
void updateModal(AppContext& ctx);
