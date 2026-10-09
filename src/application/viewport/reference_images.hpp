#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include "scene/references/reference_image.hpp"
#include "application/viewport/pictures.hpp"
#include "core/math/mat4.hpp"

struct AppContext;
class IRenderer;
class UIDrawList;

// Adds the picture as a new image facing the view at the camera's target, selected; one undo step
ReferenceHandle addReferenceImage(AppContext& ctx, std::shared_ptr<const Picture> picture, const std::string& name);

// Adds a PNG file as a new reference image, reporting to the console
bool addReferenceFrom(AppContext& ctx, const std::filesystem::path& path);

// + in the References tab: picks PNG files and adds each
void chooseReferenceImages(AppContext& ctx);

void deleteSelectedReferences(AppContext& ctx);

// Turns an image to face the view (its top toward the top of the screen), keeping where it is
Vec3 rotationFacingView(const AppContext& ctx);

// Draws the images in one depth group, farthest first so see-through ones blend over what's behind them.
// Images in the scene are sorted in with see-through objects instead, and drawn one at a time.
void drawReferenceImages(AppContext& ctx, const Mat4& viewProjection, ReferenceDepth depth);
void drawReferenceImage(AppContext& ctx, const Mat4& viewProjection, ReferenceHandle handle);

// Outlines selected images in the selection color
void drawReferenceOutlines(const AppContext& ctx, UIDrawList& ui, const Mat4& viewProjection, f32 width, f32 height);
