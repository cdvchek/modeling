#pragma once

#include "application/app_context.hpp"

// Clicks within this many pixels of a light's center pick it: the orb plus a little slack
constexpr f32 LIGHT_MARKER_PICK_RADIUS = 11.0f;

void drawLightMarkers(const AppContext& ctx, UIDrawList& ui, const Mat4& viewProjection, f32 width, f32 height);
