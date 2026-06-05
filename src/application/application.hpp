#pragma once

#include "application/app_context.hpp"

namespace Application {
    bool initialize(AppContext& ctx);
    void run(AppContext& ctx);
    void renderFrame(AppContext& ctx);
}