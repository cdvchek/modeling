#pragma once

#include "application/app_context.hpp"

void runLightCommand(AppContext& ctx, const CommandArgs& args);

// Removes every selected light as one undo step; returns false if none were selected
bool deleteSelectedLights(AppContext& ctx);
