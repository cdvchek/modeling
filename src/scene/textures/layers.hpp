#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>
#include <types>
#include "image/image.hpp"
#include "scene/pictures/picture.hpp"

// A paintable texture's pixels: transparent layers stacked in order, each cut into square tiles that copies share

inline constexpr u32 TILE_SIZE = 64;

// 8-bit RGBA, alpha not multiplied in, sRGB
using Pixel = std::array<u8, 4>;

// TILE_SIZE x TILE_SIZE pixels, rows from the top; pixels past the picture's edge are never read
struct Tile {
    std::array<u8, TILE_SIZE * TILE_SIZE * 4> pixels {};
};

// A block of pixels, from the top left
struct PixelRect {
    u32 x = 0;
    u32 y = 0;
    u32 width = 0;
    u32 height = 0;
};

// A layer's pixels as a PNG file, and the tiles it was made from: still right while the layer holds those same tiles
struct LayerPng {
    std::vector<std::shared_ptr<Tile>> tiles;
    std::vector<u8> bytes;
};

struct Layer {
    std::string name;
    bool visible = true;
    f32 opacity = 1.0f;
    // Holds the texture's file, so Reload from file replaces this layer and leaves the others
    bool fromFile = false;
    // Row by row; null is a fully transparent tile. Copies of the layer (undo) share them, so change one only through editTile
    std::vector<std::shared_ptr<Tile>> tiles;
    // The PNG last made from or read into this layer (layerPng), so saving encodes only layers that changed; copies share it
    mutable std::shared_ptr<const LayerPng> png;
};

struct LayerStack {
    u32 width = 0;      // pixels
    u32 height = 0;
    std::vector<Layer> layers;      // bottom first; none means the texture isn't layered
    u32 active = 0;                 // the layer paint goes to

    bool empty() const { return layers.empty(); }
    u32 tilesAcross() const { return (width + TILE_SIZE - 1) / TILE_SIZE; }
    u32 tilesDown() const { return (height + TILE_SIZE - 1) / TILE_SIZE; }
};

// One layer named Base holding the image; clear tiles take no memory and tiles of one color share theirs
LayerStack layersFromImage(const image::Image& image);

// The same from a PNG file, which Base keeps as its PNG; on failure error says why and out is left alone
bool layersFromPng(const std::vector<u8>& png, LayerStack& out, std::string& error);
bool layersFromPicture(const Picture& picture, LayerStack& out, std::string& error);

// A clear layer above the active one, which becomes the active one; returns its index. No name gives "Layer N"
u32 addLayer(LayerStack& stack, const std::string& name = "");

// False for the only layer (a layered texture keeps at least one) or a bad index. Removing the active layer makes the one below active
bool removeLayer(LayerStack& stack, u32 index);

// Moves a layer to another place in the order; the active layer stays the same layer
bool moveLayer(LayerStack& stack, u32 from, u32 to);

// "Layer N", one past the highest number in use
std::string nextLayerName(const LayerStack& stack);

// A pixel of one layer; clear outside the picture or for a bad layer
Pixel layerPixel(const LayerStack& stack, u32 layer, u32 x, u32 y);

// A tile to change: made if it was clear, copied first if anything else still holds it
Tile& editTile(LayerStack& stack, u32 layer, u32 tileX, u32 tileY);

// Sets every pixel of the rectangle (clipped to the picture) on one layer; clear tiles are dropped and whole tiles of the color share one
void fillLayer(LayerStack& stack, u32 layer, const PixelRect& rect, const Pixel& color);

// The visible layers blended bottom to top by their opacity, as rect.width x rect.height RGBA pixels; rect must lie inside the picture
void compositeLayers(const LayerStack& stack, const PixelRect& rect, u8* out);

// One pixel over another (src over dst, RGBA with alpha not multiplied in), with src's alpha scaled by opacity, 0 to 255
void blendPixel(u8* dst, const u8* src, u32 opacity);

// The whole combined picture
image::Image flattenLayers(const LayerStack& stack);

// One layer's own pixels, not blended with anything
image::Image layerImage(const LayerStack& stack, u32 layer);

// One layer as a PNG file: the one it already has while its tiles are the same, otherwise encoded now and kept on the layer
std::shared_ptr<const LayerPng> layerPng(const LayerStack& stack, u32 layer);

// Replaces a layer's pixels with a PNG of the stack's size, which it keeps as its PNG; false with error (and no change) otherwise
bool setLayerPng(LayerStack& stack, u32 layer, std::vector<u8> png, std::string& error);

// The combined picture as a PNG file; a lone showing layer at full opacity gives its own PNG without encoding again
std::vector<u8> flattenedPng(const LayerStack& stack);

// Where the combined picture may differ between two states of a stack (by which tiles show, not their pixels), as runs of tiles clipped to the picture
std::vector<PixelRect> changedRects(const LayerStack& before, const LayerStack& after);
