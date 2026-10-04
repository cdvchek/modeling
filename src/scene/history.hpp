#pragma once

#include <optional>
#include <vector>

#include "scene/mesh/mesh_data.hpp"
#include "scene/transform.hpp"
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

private:
    struct State {
        std::vector<MeshData> meshes;
        std::vector<Transform> transforms;
        LightCollection lights;
        Selection selection;
    };

    static State capture(const Scene& scene);
    static void restore(Scene& scene, const State& state);

    std::vector<State> m_undo;
    std::vector<State> m_redo;
    std::optional<State> m_pending;
};
