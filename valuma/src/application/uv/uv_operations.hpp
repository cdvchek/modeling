#pragma once

#include <vector>
#include "application/app_context.hpp"

// Making UVs for the object being UV-edited: seams, unwrapping, projections, and packing. Each is one undo step and
// reports what it did in the console.

// The faces unwrapping and projections work on: the selected faces (in vertex and edge mode, faces whose corners
// are all selected), or every face when none are
std::vector<FaceHandle> uvTargetFaces(const AppContext& ctx);

// Edge mode with edges selected
bool canMarkSeams(const AppContext& ctx);
// Marks (or clears) a seam on the selected edges
void markSeams(AppContext& ctx, bool seam);
// Marks a seam wherever the current UV layout is cut
void seamsFromIslands(AppContext& ctx);

// Flattens the target faces along seams. With every face, the islands are packed into the whole texture; with some,
// into the space those faces' UVs took before, so the rest of the layout stays as it is.
void unwrapUVs(AppContext& ctx);

enum class UVProjection : u8 { View, Box, Cylinder, Sphere };
// Lays the target faces out by a projection (View: as the 3D view sees them), then fits them like unwrapping does
void projectUVs(AppContext& ctx, UVProjection projection);

// Arranges every island of the object into the texture with the margin, sizes kept in proportion
void packUVs(AppContext& ctx);

bool hasUVObject(const AppContext& ctx);
