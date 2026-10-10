#pragma once

#include <string_view>
#include <vector>

#include "application/app_context.hpp"
#include "ui/radial_layout.hpp"

// One slice: runs an action or opens a submenu
struct RadialItem {
    std::string_view label;
    Action action = Action::Count;
    RadialMenuId submenu = RadialMenuId::None;
};

// Items split the circle evenly: the first is centered straight up, the rest follow clockwise
using RadialMenu = std::vector<RadialItem>;

RadialMenu buildRadialMenu(const AppContext& ctx, RadialMenuId id);

// Opens, steers, and runs the menu; true while it has the input this frame
bool updateRadialMenu(AppContext& ctx);
void drawRadialMenu(const AppContext& ctx, UIDrawList& ui);
