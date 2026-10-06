#include "scene/history.hpp"
#include "scene/scene.hpp"

#include <algorithm>

namespace {
    constexpr std::size_t MAX_STEPS = 100;
}

void History::begin(const Scene& scene) {
    if (!m_pending) m_pending = capture(scene);
}

void History::commit() {
    if (!m_pending) return;

    m_undo.push_back(std::move(*m_pending));
    m_pending.reset();
    m_redo.clear();

    if (m_undo.size() > MAX_STEPS) m_undo.erase(m_undo.begin());
}

void History::cancel(Scene& scene) {
    if (!m_pending) return;

    restore(scene, *m_pending);
    m_pending.reset();
}

bool History::undo(Scene& scene) {
    if (m_undo.empty()) return false;

    m_redo.push_back(capture(scene));
    restore(scene, m_undo.back());
    m_undo.pop_back();

    return true;
}

bool History::redo(Scene& scene) {
    if (m_redo.empty()) return false;

    m_undo.push_back(capture(scene));
    restore(scene, m_redo.back());
    m_redo.pop_back();

    return true;
}

History::State History::capture(const Scene& scene) {
    State state;

    state.objects = scene.objects;
    state.lights = scene.lights;
    state.selection = scene.selection;

    return state;
}

void History::restore(Scene& scene, const State& state) {
    scene.objects = state.objects;
    scene.objects.markAllDirty();
    scene.lights = state.lights;
    scene.selection = state.selection;
}
