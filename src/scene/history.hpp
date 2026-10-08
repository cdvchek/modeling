#pragma once

#include <optional>
#include <types>
#include <vector>

#include "scene/objects/object_collection.hpp"
#include "scene/lights/light_collection.hpp"
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

private:
    struct State {
        ObjectCollection objects;
        LightCollection lights;
        Selection selection;
        u64 id = 0;
    };

    static State capture(const Scene& scene);
    static void restore(Scene& scene, const State& state);

    std::vector<State> m_undo;
    std::vector<State> m_redo;
    std::optional<State> m_pending;

    u64 m_stateId = 0;
    u64 m_nextId = 0;
};
