#pragma once

#include "application/app_context.hpp"

// layer list | add [name] | <n> [active | remove | show | hide | opacity <v> | name <n> | move <place> | fill <r> <g> <b> [a] | clear]
// Works on the texture being painted (the Paint workspace's Texture); layer 0 is the bottom one
void runLayerCommand(AppContext& ctx, const CommandArgs& args);

// The layers of the texture being painted, made from its picture inside the caller's history step when it has none; null with a console error
LayerStack* paintLayers(AppContext& ctx);
