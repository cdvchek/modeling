#pragma once

#include "application/app_context.hpp"

// reference list | add [<file.png>] | <id> [remove | show | hide | lock | unlock | name | opacity | depth | ...]
void runReferenceCommand(AppContext& ctx, const CommandArgs& args);
