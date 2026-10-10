#pragma once

#include <filesystem>
#include <string>
#include "application/app_context.hpp"

// texture list | load <path> | <id> [remove | reload | name <n>]; material <id> map <texture id | none> is in material
void runTextureCommand(AppContext& ctx, const CommandArgs& args);

// Loads a PNG as a new texture, as one undo step, reporting to the console. With a valid material, that material uses
// it as its base color map, in the same step. Invalid when the file can't be used.
TextureHandle loadTexture(AppContext& ctx, const std::filesystem::path& path, MaterialHandle material = INVALID_MATERIAL);

// Opens the PNG dialog and loads the picked file (and assigns it to material, if valid). Called from checkActions
// when ctx.textureRequest is set, never while drawing.
void chooseTexture(AppContext& ctx, MaterialHandle material);

// The material's base color map (INVALID_TEXTURE for none), as one undo step
void setBaseColorMap(AppContext& ctx, MaterialHandle material, TextureHandle texture);

// Removes the texture as one undo step; the materials that used it go back to their plain colors
void removeTexture(AppContext& ctx, TextureHandle texture);

// Reads the texture's file again (after it was changed in another program), as one undo step; false (and a console
// error) when there's no file or it can't be read
bool reloadTexture(AppContext& ctx, TextureHandle texture);

// How many materials use the texture as a map
u32 textureUse(const AppContext& ctx, TextureHandle texture);
// "2 materials", "1 material", "unused"
std::string describeTextureUse(u32 materials);
