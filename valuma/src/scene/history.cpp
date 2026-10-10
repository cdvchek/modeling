#include "scene/history.hpp"
#include "scene/scene.hpp"

#include <algorithm>
#include <unordered_set>

namespace {
    constexpr std::size_t MAX_STEPS = 100;
}

void History::begin(const Scene& scene) {
    if (m_pending) return;

    m_pending = capture(scene);
    m_pending->id = m_stateId;
}

void History::commit() {
    if (!m_pending) return;

    m_undo.push_back(std::move(*m_pending));
    m_pending.reset();
    m_redo.clear();
    m_stateId = ++m_nextId;

    if (m_undo.size() > MAX_STEPS) m_undo.erase(m_undo.begin());

    // Painting's steps are dropped oldest first once they hold too much; the newest always stays
    while (m_undo.size() > 1 && heldPaintBytes() > m_paintBudget) m_undo.erase(m_undo.begin());
}

std::size_t History::heldPaintBytes() const {
    const auto forEachTile = [](const TextureCollection& textures, auto&& visit) {
        for (TextureHandle handle : textures.handles()) {
            for (const Layer& layer : textures.get(handle).layers.layers) {
                for (const std::shared_ptr<Tile>& tile : layer.tiles) if (tile) visit(tile.get());
            }
        }
    };
    if (m_undo.empty()) return 0;

    // Tiles the newest step has too cost nothing extra to keep in older ones
    std::unordered_set<const Tile*> newest, older;
    forEachTile(m_undo.back().textures, [&](const Tile* tile) { newest.insert(tile); });
    for (std::size_t i = 0; i + 1 < m_undo.size(); ++i) {
        forEachTile(m_undo[i].textures, [&](const Tile* tile) { if (!newest.contains(tile)) older.insert(tile); });
    }
    return older.size() * sizeof(Tile);
}

void History::cancel(Scene& scene) {
    if (!m_pending) return;

    restore(scene, *m_pending);
    m_pending.reset();
}

bool History::undo(Scene& scene) {
    if (m_undo.empty()) return false;

    m_redo.push_back(capture(scene));
    m_redo.back().id = m_stateId;
    restore(scene, m_undo.back());
    m_stateId = m_undo.back().id;
    m_undo.pop_back();

    return true;
}

bool History::redo(Scene& scene) {
    if (m_redo.empty()) return false;

    m_undo.push_back(capture(scene));
    m_undo.back().id = m_stateId;
    restore(scene, m_redo.back());
    m_stateId = m_redo.back().id;
    m_redo.pop_back();

    return true;
}

void History::clear() {
    m_undo.clear();
    m_redo.clear();
    m_pending.reset();
    m_stateId = ++m_nextId;
}

History::State History::capture(const Scene& scene) {
    State state;

    state.objects = scene.objects;
    state.lights = scene.lights;
    state.references = scene.references;
    state.materials = scene.materials;
    state.textures = scene.textures;
    state.selection = scene.selection;

    return state;
}

void History::restore(Scene& scene, const State& state) {
    scene.objects = state.objects;
    scene.objects.markAllDirty();
    scene.lights = state.lights;
    scene.references = state.references;
    scene.materials = state.materials;
    scene.textures = state.textures;
    scene.selection = state.selection;
}
