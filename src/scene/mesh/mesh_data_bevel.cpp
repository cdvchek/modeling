#include "scene/mesh/mesh_data.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace {
    enum class SpokeRole : u8 {
        Beveled,
        Slide,
        Plain
    };

    struct Slider {
        VertexHandle vertex = INVALID_VERTEX;
        Vec3 direction;
        f32 maxWidth = 0.0f;
    };

    // Slot i is the face between spoke i - 1 and spoke i.
    struct Corner {
        VertexHandle vertex;
        Vec3 position;
        bool keep = false;

        std::vector<EdgeHandle> spokes;
        std::vector<SpokeRole> roles;

        std::vector<Slider> slides;
        std::vector<Slider> insets;

        std::vector<std::vector<VertexHandle>> slotCorners;

        std::vector<VertexHandle> ring;

        bool terminal = false;
    };

    constexpr f32 MIN_SIN = 0.1f;

    u64 edgeKey(VertexHandle a, VertexHandle b) {
        return (static_cast<u64>(a.index) << 32) | b.index;
    }

    void removeRepeats(std::vector<VertexHandle>& loop) {
        loop.erase(std::unique(loop.begin(), loop.end()), loop.end());
        while (loop.size() > 1 && loop.front() == loop.back()) loop.pop_back();
    }
}

bool MeshData::bevelVertex(VertexHandle handle, BevelSession& session) {
    if (!isValidHandle(handle)) return false;
    return bevel({ handle }, {}, true, session);
}

bool MeshData::bevelEdge(EdgeHandle handle, BevelSession& session) {
    if (!isValidHandle(handle)) return false;
    return bevel({ getEdgeOrigin(handle), getEdgeTip(handle) }, { handle }, false, session);
}

bool MeshData::bevelFace(FaceHandle handle, BevelSession& session) {
    if (!isValidHandle(handle)) return false;
    return bevel(getFaceVertices(handle), getFaceEdges(handle), false, session);
}

void MeshData::setBevelWidth(const BevelSession& session, f32 width) {
    width = std::clamp(width, 0.0f, session.maxWidth);

    for (u32 i = 0; i < static_cast<u32>(session.vertices.size()); ++i) {
        Vertex* vertex = m_vertices.tryGet(session.vertices[i]);
        if (vertex) vertex->position = session.starts[i] + session.directions[i] * width;
    }

    for (VertexHandle handle : session.vertices) {
        setFacesDirtyByVertex(handle);
    }
}

void MeshData::cancelBevel(const BevelSession& session) {
    m_vertices = session.savedVertices;
    m_edges = session.savedEdges;
    m_faces = session.savedFaces;
}

bool MeshData::bevel(
    const std::vector<VertexHandle>& cornerVertices,
    const std::vector<EdgeHandle>& edges,
    bool vertexOnly,
    BevelSession& session
) {
    std::vector<EdgeHandle> beveled;

    for (EdgeHandle edge : edges) {
        beveled.push_back(edge);
        beveled.push_back(m_edges.get(edge).pair);
    }

    auto isBeveled = [&](EdgeHandle edge) {
        return std::find(beveled.begin(), beveled.end(), edge) != beveled.end();
    };

    auto isCorner = [&](VertexHandle vertex) {
        return std::find(cornerVertices.begin(), cornerVertices.end(), vertex) != cornerVertices.end();
    };

    auto direction = [&](EdgeHandle spoke) {
        return (getVertexPosition(getEdgeTip(spoke)) - getVertexPosition(getEdgeOrigin(spoke))).normalized();
    };

    auto length = [&](EdgeHandle spoke) {
        return (getVertexPosition(getEdgeTip(spoke)) - getVertexPosition(getEdgeOrigin(spoke))).length();
    };

    // 1. Classify each corner's spokes and work out how its new vertices move.
    std::vector<Corner> corners;

    for (VertexHandle vertex : cornerVertices) {
        Corner corner;
        corner.vertex = vertex;
        corner.position = getVertexPosition(vertex);
        corner.spokes = getOutgoingEdges(vertex);

        const u32 count = static_cast<u32>(corner.spokes.size());
        if (count < 3) return false;

        for (EdgeHandle spoke : corner.spokes) {
            if (isBorder(spoke)) return false;
        }

        for (u32 i = 0; i < count; ++i) {
            const EdgeHandle prev = corner.spokes[(i + count - 1) % count];
            const EdgeHandle next = corner.spokes[(i + 1) % count];

            if (vertexOnly) corner.roles.push_back(SpokeRole::Slide);
            else if (isBeveled(corner.spokes[i])) corner.roles.push_back(SpokeRole::Beveled);
            else if (isBeveled(prev) || isBeveled(next)) corner.roles.push_back(SpokeRole::Slide);
            else corner.roles.push_back(SpokeRole::Plain);
        }

        u32 plainRuns = 0;

        for (u32 i = 0; i < count; ++i) {
            const bool plain = corner.roles[i] == SpokeRole::Plain;
            const bool prevPlain = corner.roles[(i + count - 1) % count] == SpokeRole::Plain;
            if (plain && !prevPlain) ++plainRuns;
        }

        if (plainRuns > 1) return false;
        corner.keep = std::find(corner.roles.begin(), corner.roles.end(), SpokeRole::Plain) != corner.roles.end();

        corner.slides.resize(count);
        corner.insets.resize(count);

        for (u32 i = 0; i < count; ++i) {
            const u32 prev = (i + count - 1) % count;
            const u32 next = (i + 1) % count;
            const EdgeHandle spoke = corner.spokes[i];

            if (corner.roles[i] == SpokeRole::Slide) {
                const Vec3 along = direction(spoke);
                f32 speed = 1.0f;

                if (!vertexOnly) {
                    f32 total = 0.0f;
                    u32 neighbors = 0;

                    for (u32 n : { prev, next }) {
                        if (corner.roles[n] != SpokeRole::Beveled) continue;

                        const f32 sine = Vec3::cross(along, direction(corner.spokes[n])).length();
                        total += 1.0f / std::max(sine, MIN_SIN);
                        ++neighbors;
                    }

                    speed = total / static_cast<f32>(neighbors);
                }

                const f32 reach = length(spoke) * (isCorner(getEdgeTip(spoke)) ? 0.5f : 1.0f);

                corner.slides[i].direction = along * speed;
                corner.slides[i].maxWidth = reach / speed;
            }

            if (corner.roles[i] == SpokeRole::Beveled && corner.roles[prev] == SpokeRole::Beveled) {
                const Vec3 a = direction(corner.spokes[prev]);
                const Vec3 b = direction(spoke);
                const Vec3 bisector = a + b;

                if (bisector.length() < 1e-4f) return false;

                const f32 halfSine = std::sqrt(std::max(0.0f, (1.0f - Vec3::dot(a, b)) / 2.0f));
                const Vec3 move = bisector.normalized() * (1.0f / std::max(halfSine, MIN_SIN));

                f32 maxWidth = std::numeric_limits<f32>::max();

                for (u32 n : { prev, i }) {
                    const f32 along = Vec3::dot(move, direction(corner.spokes[n]));
                    if (along > 1e-6f) maxWidth = std::min(maxWidth, 0.5f * length(corner.spokes[n]) / along);
                }

                corner.insets[i].direction = move;
                corner.insets[i].maxWidth = maxWidth;
            }
        }

        corners.push_back(corner);
    }

    auto findCorner = [&](VertexHandle vertex) -> Corner* {
        for (Corner& corner : corners) {
            if (corner.vertex == vertex) return &corner;
        }
        return nullptr;
    };

    auto spokeIndex = [](const Corner& corner, EdgeHandle spoke) {
        return static_cast<u32>(std::find(corner.spokes.begin(), corner.spokes.end(), spoke) - corner.spokes.begin());
    };

    // 2. Save the mesh, then create the new vertices on top of their corners.
    session.savedVertices = m_vertices;
    session.savedEdges = m_edges;
    session.savedFaces = m_faces;

    session.vertices.clear();
    session.starts.clear();
    session.directions.clear();
    session.maxWidth = std::numeric_limits<f32>::max();

    auto addSlider = [&](Slider& slider, Vec3 position) {
        slider.vertex = addVertex(position);

        session.vertices.push_back(slider.vertex);
        session.starts.push_back(position);
        session.directions.push_back(slider.direction);
        session.maxWidth = std::min(session.maxWidth, slider.maxWidth);
    };

    for (Corner& corner : corners) {
        const u32 count = static_cast<u32>(corner.spokes.size());

        for (u32 i = 0; i < count; ++i) {
            const u32 prev = (i + count - 1) % count;

            if (corner.roles[i] == SpokeRole::Slide) addSlider(corner.slides[i], corner.position);
            if (corner.roles[i] == SpokeRole::Beveled && corner.roles[prev] == SpokeRole::Beveled) {
                addSlider(corner.insets[i], corner.position);
            }
        }

        auto element = [&](u32 i) {
            if (corner.roles[i] == SpokeRole::Slide) return corner.slides[i].vertex;
            if (corner.roles[i] == SpokeRole::Plain) return corner.vertex;
            return INVALID_VERTEX;
        };

        for (u32 i = 0; i < count; ++i) {
            const u32 prev = (i + count - 1) % count;
            std::vector<VertexHandle> slot;

            if (isValidHandle(corner.insets[i].vertex)) {
                slot.push_back(corner.insets[i].vertex);
            } else {
                for (VertexHandle vertex : { element(prev), element(i) }) {
                    if (isValidHandle(vertex) && (slot.empty() || slot.back() != vertex)) slot.push_back(vertex);
                }
            }

            corner.slotCorners.push_back(slot);
        }

        // Walking the fan backwards gives the same winding as the faces around it.
        for (u32 i = 0; i < count; ++i) {
            if (isValidHandle(corner.insets[i].vertex)) corner.ring.push_back(corner.insets[i].vertex);
            if (corner.roles[i] == SpokeRole::Slide) corner.ring.push_back(corner.slides[i].vertex);
            if (corner.roles[i] == SpokeRole::Plain) corner.ring.push_back(corner.vertex);
        }

        removeRepeats(corner.ring);
        std::reverse(corner.ring.begin(), corner.ring.end());

        corner.terminal = !vertexOnly &&
            std::count(corner.roles.begin(), corner.roles.end(), SpokeRole::Beveled) == 1;
    }

    // 3. Describe every face after the bevel.
    std::vector<FaceHandle> oldFaces;
    std::vector<std::vector<VertexHandle>> newFaces;

    for (const Corner& corner : corners) {
        for (EdgeHandle spoke : corner.spokes) {
            const FaceHandle face = m_edges.get(spoke).face;
            if (std::find(oldFaces.begin(), oldFaces.end(), face) == oldFaces.end()) oldFaces.push_back(face);
        }
    }

    // 3a. Faces around the corners: each corner is swapped for its slot's vertices.
    for (FaceHandle face : oldFaces) {
        std::vector<VertexHandle> loop;

        for (EdgeHandle incoming : getFaceEdges(face)) {
            const VertexHandle vertex = getEdgeTip(incoming);
            const Corner* corner = findCorner(vertex);

            if (!corner) {
                loop.push_back(vertex);
                continue;
            }

            const u32 slot = spokeIndex(*corner, m_edges.get(incoming).next);
            for (VertexHandle replacement : corner->slotCorners[slot]) loop.push_back(replacement);
        }

        removeRepeats(loop);
        newFaces.push_back(loop);
    }

    // 3b. One strip per beveled edge.
    auto left = [](const Corner& corner, u32 spoke) {
        return corner.slotCorners[spoke].back();
    };

    auto right = [](const Corner& corner, u32 spoke) {
        return corner.slotCorners[(spoke + 1) % corner.spokes.size()].front();
    };

    auto appendEnd = [](std::vector<VertexHandle>& loop, const Corner& corner, VertexHandle from, VertexHandle to) {
        loop.push_back(from);
        if (!corner.terminal) return;

        const std::vector<VertexHandle>& ring = corner.ring;
        const u32 count = static_cast<u32>(ring.size());
        const u32 start = static_cast<u32>(std::find(ring.begin(), ring.end(), from) - ring.begin());

        for (u32 i = 1; i < count; ++i) {
            const VertexHandle vertex = ring[(start + i) % count];
            if (vertex == to) break;
            loop.push_back(vertex);
        }
    };

    for (EdgeHandle edge : edges) {
        const EdgeHandle pair = m_edges.get(edge).pair;
        const Corner* start = findCorner(getEdgeOrigin(edge));
        const Corner* end = findCorner(getEdgeTip(edge));

        const u32 startSpoke = spokeIndex(*start, edge);
        const u32 endSpoke = spokeIndex(*end, pair);

        std::vector<VertexHandle> strip;
        strip.push_back(right(*start, startSpoke));
        appendEnd(strip, *end, left(*end, endSpoke), right(*end, endSpoke));
        strip.push_back(right(*end, endSpoke));
        appendEnd(strip, *start, left(*start, startSpoke), right(*start, startSpoke));

        removeRepeats(strip);
        newFaces.push_back(strip);
    }

    // 3c. A face filling each corner that isn't closed by a strip.
    for (const Corner& corner : corners) {
        if (!corner.terminal && corner.ring.size() >= 3) newFaces.push_back(corner.ring);
    }

    // 4. Swap in the new faces, then drop corners nothing uses anymore.
    if (!replaceFaces(oldFaces, newFaces)) {
        cancelBevel(session);
        return false;
    }

    for (const Corner& corner : corners) {
        if (!corner.keep) m_vertices.remove(corner.vertex);
    }

    return true;
}

bool MeshData::replaceFaces(
    const std::vector<FaceHandle>& oldFaces,
    const std::vector<std::vector<VertexHandle>>& newFaces
) {
    auto isOld = [&](FaceHandle face) {
        return std::find(oldFaces.begin(), oldFaces.end(), face) != oldFaces.end();
    };

    // 1. Remember the half-edges just outside the region.
    std::map<u64, EdgeHandle> outside;
    std::vector<EdgeHandle> removed;

    for (FaceHandle face : oldFaces) {
        for (EdgeHandle edge : getFaceEdges(face)) {
            removed.push_back(edge);

            const EdgeHandle pair = m_edges.get(edge).pair;
            if (!isOld(m_edges.get(pair).face)) {
                outside[edgeKey(getEdgeOrigin(edge), getEdgeTip(edge))] = pair;
            }
        }
    }

    // 2. Remove the old faces and their half-edges.
    for (EdgeHandle edge : removed) m_edges.remove(edge);
    for (FaceHandle face : oldFaces) m_faces.remove(face);

    // 3. Build each new face's loop.
    std::map<u64, EdgeHandle> created;

    for (const std::vector<VertexHandle>& loop : newFaces) {
        const u32 count = static_cast<u32>(loop.size());
        if (count < 3) return false;

        const FaceHandle face = m_faces.insert(Face{});
        std::vector<EdgeHandle> halves;

        for (u32 i = 0; i < count; ++i) {
            Edge edge;
            edge.tip = loop[(i + 1) % count];
            edge.face = face;

            const EdgeHandle handle = m_edges.insert(edge);

            if (!created.emplace(edgeKey(loop[i], edge.tip), handle).second) return false;

            halves.push_back(handle);
        }

        for (u32 i = 0; i < count; ++i) {
            link(halves[i], halves[(i + 1) % count]);
            m_vertices.get(loop[i]).edge = halves[i];
        }

        m_faces.get(face).edge = halves[0];
    }

    // 4. Pair every new half-edge, with another new one or with the outside.
    u32 outsideUsed = 0;

    for (const auto& [key, handle] : created) {
        const u64 reverse = (key << 32) | (key >> 32);

        auto twin = created.find(reverse);
        if (twin != created.end()) {
            m_edges.get(handle).pair = twin->second;
            continue;
        }

        auto outer = outside.find(key);
        if (outer == outside.end()) return false;

        m_edges.get(handle).pair = outer->second;
        m_edges.get(outer->second).pair = handle;
        ++outsideUsed;
    }

    return outsideUsed == outside.size();
}
