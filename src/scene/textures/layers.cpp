#include "scene/textures/layers.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
    constexpr const char* BASE_NAME = "Base";
    constexpr const char* LAYER_PREFIX = "Layer ";

    std::shared_ptr<Tile> solidTile(const Pixel& color) {
        auto tile = std::make_shared<Tile>();
        for (std::size_t i = 0; i < tile->pixels.size(); i += 4) std::copy(color.begin(), color.end(), tile->pixels.begin() + i);
        return tile;
    }

    Layer clearLayer(const LayerStack& stack, const std::string& name) {
        Layer layer;
        layer.name = name;
        layer.tiles.resize(std::size_t(stack.tilesAcross()) * stack.tilesDown());
        return layer;
    }

    // A layer's opacity in 255ths; 0 when it's hidden
    u32 showingOpacity(const Layer& layer) {
        return layer.visible ? static_cast<u32>(std::lround(std::clamp(layer.opacity, 0.0f, 1.0f) * 255.0f)) : 0;
    }

    // The first layer from index up that puts something in the tile
    std::size_t nextShowing(const LayerStack& stack, std::size_t index, std::size_t tile) {
        while (index < stack.layers.size() && (showingOpacity(stack.layers[index]) == 0 || !stack.layers[index].tiles[tile])) index++;
        return index;
    }

    // The same tiles show in the same order at the same opacity
    bool sameTile(const LayerStack& before, const LayerStack& after, std::size_t tile) {
        std::size_t a = nextShowing(before, 0, tile), b = nextShowing(after, 0, tile);
        while (a < before.layers.size() && b < after.layers.size()) {
            const Layer& one = before.layers[a];
            const Layer& other = after.layers[b];
            if (one.tiles[tile] != other.tiles[tile] || showingOpacity(one) != showingOpacity(other)) return false;
            a = nextShowing(before, a + 1, tile);
            b = nextShowing(after, b + 1, tile);
        }
        return a == before.layers.size() && b == after.layers.size();
    }

    // src over dst, both with alpha not multiplied in; opacity is 0 to 255
    void blend(u8* dst, const u8* src, u32 opacity) {
        const u32 srcAlpha = (src[3] * opacity + 127) / 255;
        if (srcAlpha == 0) return;
        const u32 dstAlpha = dst[3];
        if (srcAlpha == 255 || dstAlpha == 0) {
            std::copy(src, src + 3, dst);
            dst[3] = static_cast<u8>(srcAlpha);
            return;
        }

        // Weights in 255ths of 255ths: what the source covers, and what still shows of the destination
        const u32 srcWeight = srcAlpha * 255;
        const u32 dstWeight = dstAlpha * (255 - srcAlpha);
        const u32 total = srcWeight + dstWeight;
        for (int channel = 0; channel < 3; ++channel) {
            dst[channel] = static_cast<u8>((src[channel] * srcWeight + dst[channel] * dstWeight + total / 2) / total);
        }
        dst[3] = static_cast<u8>((total + 127) / 255);
    }

    // An image cut into tiles: clear tiles stay null, and whole tiles of one color share one
    std::vector<std::shared_ptr<Tile>> tilesFromImage(const image::Image& image) {
        const u32 across = (image.width + TILE_SIZE - 1) / TILE_SIZE, down = (image.height + TILE_SIZE - 1) / TILE_SIZE;
        std::vector<std::shared_ptr<Tile>> tiles(std::size_t(across) * down);
        std::shared_ptr<Tile> lastSolid;
        for (u32 tileY = 0; tileY < down; ++tileY) {
            for (u32 tileX = 0; tileX < across; ++tileX) {
                const u32 left = tileX * TILE_SIZE, top = tileY * TILE_SIZE;
                const u32 columns = std::min(TILE_SIZE, image.width - left), rows = std::min(TILE_SIZE, image.height - top);
                const u8* first = image.pixels.data() + (std::size_t(top) * image.width + left) * 4;

                // Clear tiles stay null; a whole tile of one color can share the last such tile
                bool clear = true, solid = columns == TILE_SIZE && rows == TILE_SIZE;
                for (u32 y = 0; y < rows; ++y) {
                    const u8* row = first + std::size_t(y) * image.width * 4;
                    for (u32 x = 0; x < columns; ++x) {
                        clear = clear && row[x * 4 + 3] == 0;
                        solid = solid && std::memcmp(row + x * 4, first, 4) == 0;
                    }
                }
                if (clear) continue;

                std::shared_ptr<Tile>& tile = tiles[std::size_t(tileY) * across + tileX];
                if (solid && lastSolid && std::memcmp(lastSolid->pixels.data(), first, 4) == 0) {
                    tile = lastSolid;
                    continue;
                }
                tile = std::make_shared<Tile>();
                for (u32 y = 0; y < rows; ++y) {
                    std::memcpy(tile->pixels.data() + std::size_t(y) * TILE_SIZE * 4, first + std::size_t(y) * image.width * 4, std::size_t(columns) * 4);
                }
                if (solid) lastSolid = tile;
            }
        }
        return tiles;
    }
}

LayerStack layersFromImage(const image::Image& image) {
    LayerStack stack;
    stack.width = image.width;
    stack.height = image.height;
    Layer base;
    base.name = BASE_NAME;
    base.tiles = tilesFromImage(image);
    stack.layers.push_back(std::move(base));
    return stack;
}

bool layersFromPng(const std::vector<u8>& png, LayerStack& out, std::string& error) {
    image::Image image;
    if (!image::decodePng(png.data(), png.size(), image, error)) return false;
    out = layersFromImage(image);
    out.layers[0].png = std::make_shared<LayerPng>(LayerPng { out.layers[0].tiles, png });
    return true;
}

bool layersFromPicture(const Picture& picture, LayerStack& out, std::string& error) {
    return layersFromPng(picture.png, out, error);
}

u32 addLayer(LayerStack& stack, const std::string& name) {
    const u32 index = stack.empty() ? 0 : std::min(stack.active + 1, static_cast<u32>(stack.layers.size()));
    stack.layers.insert(stack.layers.begin() + index, clearLayer(stack, name.empty() ? nextLayerName(stack) : name));
    stack.active = index;
    return index;
}

bool removeLayer(LayerStack& stack, u32 index) {
    if (stack.layers.size() <= 1 || index >= stack.layers.size()) return false;
    stack.layers.erase(stack.layers.begin() + index);
    if (stack.active >= index && stack.active > 0) stack.active--;
    return true;
}

bool moveLayer(LayerStack& stack, u32 from, u32 to) {
    const u32 count = static_cast<u32>(stack.layers.size());
    if (from >= count || to >= count) return false;
    if (from == to) return true;

    Layer moved = std::move(stack.layers[from]);
    stack.layers.erase(stack.layers.begin() + from);
    stack.layers.insert(stack.layers.begin() + to, std::move(moved));

    // The active layer follows its layer: the moved one, or one the move slid past
    if (stack.active == from) stack.active = to;
    else if (from < stack.active && stack.active <= to) stack.active--;
    else if (to <= stack.active && stack.active < from) stack.active++;
    return true;
}

std::string nextLayerName(const LayerStack& stack) {
    const std::size_t prefix = std::strlen(LAYER_PREFIX);
    u32 highest = 0;
    for (const Layer& layer : stack.layers) {
        if (layer.name.size() <= prefix || layer.name.size() > prefix + 9 || layer.name.compare(0, prefix, LAYER_PREFIX) != 0) continue;
        const std::string digits = layer.name.substr(prefix);
        if (!std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; })) continue;
        highest = std::max(highest, static_cast<u32>(std::stoul(digits)));
    }
    return LAYER_PREFIX + std::to_string(highest + 1);
}

Pixel layerPixel(const LayerStack& stack, u32 layer, u32 x, u32 y) {
    if (layer >= stack.layers.size() || x >= stack.width || y >= stack.height) return {};
    const std::shared_ptr<Tile>& tile = stack.layers[layer].tiles[std::size_t(y / TILE_SIZE) * stack.tilesAcross() + x / TILE_SIZE];
    if (!tile) return {};
    const u8* pixel = tile->pixels.data() + (std::size_t(y % TILE_SIZE) * TILE_SIZE + x % TILE_SIZE) * 4;
    return { pixel[0], pixel[1], pixel[2], pixel[3] };
}

Tile& editTile(LayerStack& stack, u32 layer, u32 tileX, u32 tileY) {
    std::shared_ptr<Tile>& tile = stack.layers[layer].tiles[std::size_t(tileY) * stack.tilesAcross() + tileX];
    if (!tile) tile = std::make_shared<Tile>();
    else if (tile.use_count() > 1) tile = std::make_shared<Tile>(*tile);
    return *tile;
}

void fillLayer(LayerStack& stack, u32 layer, const PixelRect& rect, const Pixel& color) {
    if (layer >= stack.layers.size() || rect.x >= stack.width || rect.y >= stack.height || rect.width == 0 || rect.height == 0) return;
    const u32 right = rect.x + std::min(rect.width, stack.width - rect.x), bottom = rect.y + std::min(rect.height, stack.height - rect.y);

    std::shared_ptr<Tile> solid;
    for (u32 tileY = rect.y / TILE_SIZE; tileY * TILE_SIZE < bottom; ++tileY) {
        for (u32 tileX = rect.x / TILE_SIZE; tileX * TILE_SIZE < right; ++tileX) {
            const u32 tileLeft = tileX * TILE_SIZE, tileTop = tileY * TILE_SIZE;
            const u32 x0 = std::max(rect.x, tileLeft), x1 = std::min(right, tileLeft + TILE_SIZE);
            const u32 y0 = std::max(rect.y, tileTop), y1 = std::min(bottom, tileTop + TILE_SIZE);

            // A tile filled over all of its part of the picture needs no pixels of its own
            const bool whole = x0 == tileLeft && y0 == tileTop && x1 == std::min(stack.width, tileLeft + TILE_SIZE) && y1 == std::min(stack.height, tileTop + TILE_SIZE);
            if (whole) {
                if (color[3] != 0 && !solid) solid = solidTile(color);
                stack.layers[layer].tiles[std::size_t(tileY) * stack.tilesAcross() + tileX] = color[3] != 0 ? solid : nullptr;
                continue;
            }

            Tile& tile = editTile(stack, layer, tileX, tileY);
            for (u32 y = y0; y < y1; ++y) {
                u8* row = tile.pixels.data() + (std::size_t(y - tileTop) * TILE_SIZE + (x0 - tileLeft)) * 4;
                for (u32 x = x0; x < x1; ++x, row += 4) std::copy(color.begin(), color.end(), row);
            }
        }
    }
}

void compositeLayers(const LayerStack& stack, const PixelRect& rect, u8* out) {
    std::memset(out, 0, std::size_t(rect.width) * rect.height * 4);
    if (rect.width == 0 || rect.height == 0) return;
    const u32 right = rect.x + rect.width, bottom = rect.y + rect.height;

    for (const Layer& layer : stack.layers) {
        const u32 opacity = showingOpacity(layer);
        if (opacity == 0) continue;

        for (u32 tileY = rect.y / TILE_SIZE; tileY * TILE_SIZE < bottom; ++tileY) {
            for (u32 tileX = rect.x / TILE_SIZE; tileX * TILE_SIZE < right; ++tileX) {
                const std::shared_ptr<Tile>& tile = layer.tiles[std::size_t(tileY) * stack.tilesAcross() + tileX];
                if (!tile) continue;

                const u32 tileLeft = tileX * TILE_SIZE, tileTop = tileY * TILE_SIZE;
                const u32 x0 = std::max(rect.x, tileLeft), x1 = std::min(right, tileLeft + TILE_SIZE);
                const u32 y0 = std::max(rect.y, tileTop), y1 = std::min(bottom, tileTop + TILE_SIZE);
                for (u32 y = y0; y < y1; ++y) {
                    const u8* src = tile->pixels.data() + (std::size_t(y - tileTop) * TILE_SIZE + (x0 - tileLeft)) * 4;
                    u8* dst = out + (std::size_t(y - rect.y) * rect.width + (x0 - rect.x)) * 4;
                    for (u32 x = x0; x < x1; ++x, src += 4, dst += 4) blend(dst, src, opacity);
                }
            }
        }
    }
}

image::Image flattenLayers(const LayerStack& stack) {
    image::Image image;
    image.width = stack.width;
    image.height = stack.height;
    image.pixels.resize(std::size_t(stack.width) * stack.height * 4);
    compositeLayers(stack, { 0, 0, stack.width, stack.height }, image.pixels.data());
    return image;
}

std::vector<PixelRect> changedRects(const LayerStack& before, const LayerStack& after) {
    if (before.width != after.width || before.height != after.height) {
        if (after.width == 0 || after.height == 0) return {};
        return { { 0, 0, after.width, after.height } };
    }

    // Changed tiles next to each other in a row go out as one rectangle
    std::vector<PixelRect> rects;
    const u32 across = after.tilesAcross();
    for (u32 tileY = 0; tileY < after.tilesDown(); ++tileY) {
        const u32 top = tileY * TILE_SIZE;
        const u32 height = std::min(TILE_SIZE, after.height - top);
        for (u32 tileX = 0; tileX < across; ++tileX) {
            if (sameTile(before, after, std::size_t(tileY) * across + tileX)) continue;
            const u32 first = tileX;
            while (tileX + 1 < across && !sameTile(before, after, std::size_t(tileY) * across + tileX + 1)) tileX++;
            const u32 left = first * TILE_SIZE;
            rects.push_back({ left, top, std::min((tileX + 1) * TILE_SIZE, after.width) - left, height });
        }
    }
    return rects;
}

image::Image layerImage(const LayerStack& stack, u32 index) {
    image::Image image;
    image.width = stack.width;
    image.height = stack.height;
    image.pixels.resize(std::size_t(stack.width) * stack.height * 4);
    if (index >= stack.layers.size()) return image;

    const Layer& layer = stack.layers[index];
    for (u32 tileY = 0; tileY < stack.tilesDown(); ++tileY) {
        for (u32 tileX = 0; tileX < stack.tilesAcross(); ++tileX) {
            const std::shared_ptr<Tile>& tile = layer.tiles[std::size_t(tileY) * stack.tilesAcross() + tileX];
            if (!tile) continue;
            const u32 left = tileX * TILE_SIZE, top = tileY * TILE_SIZE;
            const u32 columns = std::min(TILE_SIZE, stack.width - left), rows = std::min(TILE_SIZE, stack.height - top);
            for (u32 y = 0; y < rows; ++y) {
                std::memcpy(image.pixels.data() + (std::size_t(top + y) * stack.width + left) * 4, tile->pixels.data() + std::size_t(y) * TILE_SIZE * 4, std::size_t(columns) * 4);
            }
        }
    }
    return image;
}

std::shared_ptr<const LayerPng> layerPng(const LayerStack& stack, u32 index) {
    const Layer& layer = stack.layers[index];
    if (!layer.png || layer.png->tiles != layer.tiles) {
        layer.png = std::make_shared<LayerPng>(LayerPng { layer.tiles, image::encodePng(layerImage(stack, index)) });
    }
    return layer.png;
}

bool setLayerPng(LayerStack& stack, u32 index, std::vector<u8> png, std::string& error) {
    image::Image image;
    if (index >= stack.layers.size()) {
        error = "there's no such layer";
        return false;
    }
    if (!image::decodePng(png.data(), png.size(), image, error)) return false;
    if (image.width != stack.width || image.height != stack.height) {
        error = "it's " + std::to_string(image.width) + " x " + std::to_string(image.height) + ", and the texture is "
              + std::to_string(stack.width) + " x " + std::to_string(stack.height);
        return false;
    }

    Layer& layer = stack.layers[index];
    layer.tiles = tilesFromImage(image);
    layer.png = std::make_shared<LayerPng>(LayerPng { layer.tiles, std::move(png) });
    return true;
}

std::vector<u8> flattenedPng(const LayerStack& stack) {
    // What shows, and whether it's a single layer as it is
    u32 showing = 0, only = 0;
    for (u32 i = 0; i < stack.layers.size(); ++i) {
        if (showingOpacity(stack.layers[i]) == 0) continue;
        showing++;
        only = i;
    }
    if (showing == 1 && showingOpacity(stack.layers[only]) == 255) return layerPng(stack, only)->bytes;
    return image::encodePng(flattenLayers(stack));
}

void blendPixel(u8* dst, const u8* src, u32 opacity) {
    blend(dst, src, opacity);
}
