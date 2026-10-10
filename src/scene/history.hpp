#pragma once

#include <cstddef>
#include <optional>
#include <types>
#include <vector>

#include "scene/objects/object_collection.hpp"
#include "scene/lights/light_collection.hpp"
#include "scene/references/reference_collection.hpp"
#include "scene/materials/material_collection.hpp"
#include "scene/textures/texture_collection.hpp"
#include "scene/selection/selection.hpp"

struct Scene;

class History {
public:
    // Call begin before changing the scene, then commit to keep the change or cancel to restore it.
    void begin(const Scene& scene);
    void commit();
    void cancel(Scene& scene);

    bool undo(Scene& scene);
    bool redo(Scene& scene);

    bool canUndo() const { return !m_undo.empty(); }
    bool canRedo() const { return !m_redo.empty(); }

    // Names the scene's current state: a commit gives a new id, and undo or redo back to a state gives its id again,
    // so comparing with the id saved earlier tells whether anything changed since
    u64 stateId() const { return m_stateId; }

    // Forgets every step (after opening or starting a project); the state gets a new id
    void clear();

    // The most memory undo steps may hold in paint tiles the newest step no longer has (256 MB unless set); the oldest steps go first
    void setPaintBudget(std::size_t bytes) { m_paintBudget = bytes; }
    std::size_t undoSteps() const { return m_undo.size(); }

private:
    struct State {
        ObjectCollection objects;
        LightCollection lights;
        ReferenceCollection references;   // pictures are shared, so this copy is cheap
        MaterialCollection materials;
        TextureCollection textures;       // pictures and layer tiles are shared, so this copy is cheap
        Selection selection;
        u64 id = 0;
    };

    // Bytes of paint tiles held only by steps older than the newest
    std::size_t heldPaintBytes() const;

    static State capture(const Scene& scene);
    static void restore(Scene& scene, const State& state);

    std::vector<State> m_undo;
    std::vector<State> m_redo;
    std::optional<State> m_pending;

    std::size_t m_paintBudget = std::size_t(256) * 1024 * 1024;

    u64 m_stateId = 0;
    u64 m_nextId = 0;
};
