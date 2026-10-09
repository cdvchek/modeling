#pragma once

#include "application/app_context.hpp"

// Call right after a widget: turns one press-to-release interaction into a single undo step
inline void trackUndo(AppContext& ctx) {
    if (ctx.ui.isItemActivated()) ctx.history.begin(ctx.scene);

    if (ctx.ui.isItemDeactivated()) {
        if (ctx.ui.isItemDeactivatedAfterEdit()) ctx.history.commit();
        else ctx.history.cancel(ctx.scene);
    }
}
