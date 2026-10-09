#pragma once

#include "application/app_context.hpp"

// Clicks within this many pixels of an origin's dot pick it
constexpr f32 ORIGIN_MARKER_PICK_RADIUS = 9.0f;

// Object mode: a faint dashed line from each child's origin to its parent's, so you can see what's attached
void drawParentLines(const AppContext& ctx, UIDrawList& ui, const Mat4& viewProjection, f32 width, f32 height);

// Every object's origin as a dot on top of the scene: the active object's bright, the others greyed,
// a selected one purple with short lines along its own +X, +Y, +Z. Hidden when the View menu turns origins off.
void drawOriginMarkers(const AppContext& ctx, UIDrawList& ui, const Mat4& viewProjection, f32 width, f32 height);
