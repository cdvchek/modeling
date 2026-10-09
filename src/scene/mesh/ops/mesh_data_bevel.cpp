#include "scene/mesh/mesh_data.hpp"
#include "core/math/vec4.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>

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
    // On a border vertex spoke 0 is the border half-edge with no face, so slot 0 is the gap where there's no face.
    struct Corner {
        VertexHandle vertex;
        Vec3 position;
        bool keep = false;
        bool border = false;

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

    Vec3 transformPoint(const Mat4& matrix, Vec3 point) {
        const Vec4 result = matrix * Vec4(point.x, point.y, point.z, 1.0f);
        return Vec3(result.x, result.y, result.z);
    }

    Vec3 transformDirection(const Mat4& matrix, Vec3 direction) {
        const Vec4 result = matrix * Vec4(direction.x, direction.y, direction.z, 0.0f);
        return Vec3(result.x, result.y, result.z);
    }

    void removeRepeats(std::vector<VertexHandle>& loop) {
        loop.erase(std::unique(loop.begin(), loop.end()), loop.end());
        while (loop.size() > 1 && loop.front() == loop.back()) loop.pop_back();
    }
}

bool MeshData::bevelVertex(VertexHandle handle, SlideSession& session, const Mat4& space) {
    if (!isValidHandle(handle)) return false;
    return runInSpace(session, space, [&] { return bevel({ handle }, {}, true, session); });
}

bool MeshData::bevelEdge(EdgeHandle handle, SlideSession& session, const Mat4& space) {
    if (!isValidHandle(handle)) return false;
    return runInSpace(session, space, [&] { return bevel({ getEdgeOrigin(handle), getEdgeTip(handle) }, { handle }, false, session); });
}

bool MeshData::bevelFace(FaceHandle handle, SlideSession& session, const Mat4& space) {
    if (!isValidHandle(handle)) return false;
    return runInSpace(session, space, [&] { return bevel(getFaceVertices(handle), getFaceEdges(handle), false, session); });
}

bool MeshData::runInSpace(SlideSession& session, const Mat4& space, const std::function<bool()>& op) {
    // Exact copy, so existing vertices come back without rounding error
    const DynamicArray<Vertex, VertexHandle> original = m_vertices;
    const Mat4 back = Mat4::inverse(space);

    for (VertexHandle handle : m_vertices.getActiveHandles()) {
        Vertex& vertex = m_vertices.get(handle);
        vertex.position = transformPoint(space, vertex.position);
    }

    const bool done = op();

    for (VertexHandle handle : m_vertices.getActiveHandles()) {
        Vertex& vertex = m_vertices.get(handle);
        const Vertex* before = original.tryGet(handle);
        vertex.position = before ? before->position : transformPoint(back, vertex.position);
    }

    if (!done) return false;

    // Slides start and move in mesh space; a width still moves them that far in the bevel's space
    session.savedVertices = original;
    for (Vec3& start : session.starts) start = transformPoint(back, start);
    for (Vec3& direction : session.directions) direction = transformDirection(back, direction);

    return true;
}

void MeshData::setSlideWidth(const SlideSession& session, f32 width) {
    width = std::clamp(width, 0.0f, session.maxWidth);

    for (u32 i = 0; i < static_cast<u32>(session.vertices.size()); ++i) {
        positionVertex(session.vertices[i], session.starts[i] + session.directions[i] * width);
    }

    for (VertexHandle handle : session.vertices) {
        setFacesDirtyByVertex(handle);
    }
}

void MeshData::cancelSlide(const SlideSession& session) {
    m_vertices = session.savedVertices;
    m_edges = session.savedEdges;
    m_faces = session.savedFaces;
    markAllMoved();
}

bool MeshData::bevel(
    const std::vector<VertexHandle>& cornerVertices,
    const std::vector<EdgeHandle>& edges,
    bool vertexOnly,
    SlideSession& session
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

    // An inset vertex sits between two beveled spokes, inside the face they share (never in a border gap)
    auto hasInset = [](const Corner& corner, u32 i) {
        const u32 count = static_cast<u32>(corner.spokes.size());
        if (corner.border && i == 0) return false;
        return corner.roles[i] == SpokeRole::Beveled && corner.roles[(i + count - 1) % count] == SpokeRole::Beveled;
    };

    // 1. Classify each corner's spokes and work out how its new vertices move.
    std::vector<Corner> corners;
    bool hasBorder = false;

    for (VertexHandle vertex : cornerVertices) {
        Corner corner;
        corner.vertex = vertex;
        corner.position = getVertexPosition(vertex);
        corner.spokes = getOutgoingEdges(vertex);

        const u32 count = static_cast<u32>(corner.spokes.size());

        // A border vertex's fan is open: start it at the gap so it runs from one border edge to the other
        const auto gap = std::find_if(corner.spokes.begin(), corner.spokes.end(), [&](EdgeHandle spoke) { return isBorder(spoke); });
        if (gap != corner.spokes.end()) {
            if (std::count_if(corner.spokes.begin(), corner.spokes.end(), [&](EdgeHandle spoke) { return isBorder(spoke); }) > 1) return false;
            std::rotate(corner.spokes.begin(), gap, corner.spokes.end());
            corner.border = true;
            hasBorder = true;
        }

        if (count < (corner.border ? 2u : 3u)) return false;

        // Spokes on either side of the gap don't share a face, so they aren't neighbors
        auto prevOf = [&](u32 i) { return corner.border && i == 0 ? -1 : static_cast<i32>((i + count - 1) % count); };
        auto nextOf = [&](u32 i) { return corner.border && i == count - 1 ? -1 : static_cast<i32>((i + 1) % count); };
        auto beveledAt = [&](i32 i) { return i >= 0 && isBeveled(corner.spokes[i]); };

        for (u32 i = 0; i < count; ++i) {
            if (vertexOnly) corner.roles.push_back(SpokeRole::Slide);
            else if (isBeveled(corner.spokes[i])) corner.roles.push_back(SpokeRole::Beveled);
            else if (beveledAt(prevOf(i)) || beveledAt(nextOf(i))) corner.roles.push_back(SpokeRole::Slide);
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

        // A beveled border edge keeps its corner on the gap side, so the outline doesn't move
        if (corner.border && (corner.roles.front() == SpokeRole::Beveled || corner.roles.back() == SpokeRole::Beveled)) corner.keep = true;

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

                    for (i32 n : { prevOf(i), nextOf(i) }) {
                        if (n < 0 || corner.roles[n] != SpokeRole::Beveled) continue;

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

            if (hasInset(corner, i)) {
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
            if (hasInset(corner, i)) addSlider(corner.insets[i], corner.position);
        }

        auto element = [&](u32 i) {
            if (corner.roles[i] == SpokeRole::Slide) return corner.slides[i].vertex;
            if (corner.roles[i] == SpokeRole::Plain) return corner.vertex;
            return INVALID_VERTEX;
        };

        for (u32 i = 0; i < count; ++i) {
            const u32 prev = (i + count - 1) % count;
            std::vector<VertexHandle> slot;

            if (corner.border && i == 0) {
                slot.push_back(corner.vertex);
            } else if (isValidHandle(corner.insets[i].vertex)) {
                slot.push_back(corner.insets[i].vertex);
            } else {
                for (VertexHandle vertex : { element(prev), element(i) }) {
                    if (isValidHandle(vertex) && (slot.empty() || slot.back() != vertex)) slot.push_back(vertex);
                }
            }

            corner.slotCorners.push_back(slot);
        }

        // Walking the fan backwards gives the same winding as the faces around it.
        // A kept border corner sits in the gap, so the fill closes against it instead of leaving a hole.
        const bool keptForBorder = corner.border && (corner.roles.front() == SpokeRole::Beveled || corner.roles.back() == SpokeRole::Beveled);

        for (u32 i = 0; i < count; ++i) {
            if (i == 0 && keptForBorder) corner.ring.push_back(corner.vertex);
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
            if (!m_faces.isValid(face)) continue;
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
    if (!replaceFaces(oldFaces, newFaces, nullptr, hasBorder)) {
        cancelSlide(session);
        return false;
    }

    for (const Corner& corner : corners) {
        if (!corner.keep) m_vertices.remove(corner.vertex);
    }

    return true;
}

bool MeshData::replaceFaces(
    const std::vector<FaceHandle>& oldFaces,
    const std::vector<std::vector<VertexHandle>>& newFaces,
    std::vector<FaceHandle>* createdFaces,
    bool allowBorders
) {
    auto isOld = [&](FaceHandle face) {
        return std::find(oldFaces.begin(), oldFaces.end(), face) != oldFaces.end();
    };

    // 1. Remember the half-edges just outside the region.
    std::map<u64, EdgeHandle> outside;
    std::vector<EdgeHandle> removed;

    // Edge marks by their ends, so a rebuilt edge between the same two vertices keeps its mark
    std::map<u64, EdgeMark> oldMarks;
    std::set<u64> oldSeams;

    for (FaceHandle face : oldFaces) {
        for (EdgeHandle edge : getFaceEdges(face)) {
            removed.push_back(edge);
            if (m_edges.get(edge).mark != EdgeMark::None) oldMarks[edgeKey(getEdgeOrigin(edge), getEdgeTip(edge))] = m_edges.get(edge).mark;
            if (m_edges.get(edge).seam) oldSeams.insert(edgeKey(getEdgeOrigin(edge), getEdgeTip(edge)));

            const EdgeHandle pair = m_edges.get(edge).pair;
            if (!isOld(m_edges.get(pair).face)) {
                outside[edgeKey(getEdgeOrigin(edge), getEdgeTip(edge))] = pair;
            }
        }
    }

    // 2. Remove the old faces and their half-edges, keeping their materials and corner UVs for the new faces.
    std::vector<MaterialHandle> oldMaterials;
    for (FaceHandle face : oldFaces) oldMaterials.push_back(m_faces.get(face).material);
    const MaterialHandle shared = sharedFaceMaterial(oldFaces);

    struct OldCorner {
        VertexHandle vertex;
        Vec3 position;
        Vec2 uv;
    };
    std::vector<std::vector<OldCorner>> oldCorners;
    for (FaceHandle face : oldFaces) {
        std::vector<OldCorner> corners;
        for (EdgeHandle edge : getFaceEdges(face)) {
            const Edge& half = m_edges.get(edge);
            corners.push_back({ half.tip, m_vertices.get(half.tip).position, half.uv });
        }
        oldCorners.push_back(std::move(corners));
    }

    // A new corner's UV: the old corner at the same vertex, or at the same spot (copies are made where their
    // originals are), in the face it came from first; failing that any old face; failing that the nearest corner
    const auto findIn = [&](const std::vector<OldCorner>& corners, VertexHandle vertex, const Vec3& position, Vec2& uv) {
        for (const OldCorner& corner : corners) {
            if (corner.vertex == vertex) { uv = corner.uv; return true; }
        }
        for (const OldCorner& corner : corners) {
            if (corner.position.x == position.x && corner.position.y == position.y && corner.position.z == position.z) { uv = corner.uv; return true; }
        }
        return false;
    };
    const auto cornerUV = [&](std::size_t reference, VertexHandle vertex) {
        const Vec3 position = m_vertices.get(vertex).position;
        Vec2 uv;
        if (reference < oldCorners.size() && findIn(oldCorners[reference], vertex, position, uv)) return uv;
        for (const std::vector<OldCorner>& corners : oldCorners) {
            if (findIn(corners, vertex, position, uv)) return uv;
        }
        f32 best = std::numeric_limits<f32>::max();
        for (const std::vector<OldCorner>& corners : oldCorners) {
            for (const OldCorner& corner : corners) {
                const f32 distance = (corner.position - position).length();
                if (distance < best) {
                    best = distance;
                    uv = corner.uv;
                }
            }
        }
        return uv;
    };
    // The old face a new one shares the most corners with
    const auto referenceFor = [&](const std::vector<VertexHandle>& loop) {
        std::size_t best = 0;
        u32 bestShared = 0;
        for (std::size_t f = 0; f < oldCorners.size(); ++f) {
            u32 sharedCorners = 0;
            Vec2 unused;
            for (VertexHandle vertex : loop) {
                if (findIn(oldCorners[f], vertex, m_vertices.get(vertex).position, unused)) ++sharedCorners;
            }
            if (sharedCorners > bestShared) {
                bestShared = sharedCorners;
                best = f;
            }
        }
        return best;
    };

    for (EdgeHandle edge : removed) m_edges.remove(edge);
    for (FaceHandle face : oldFaces) m_faces.remove(face);

    // 3. Build each new face's loop.
    std::map<u64, EdgeHandle> created;

    for (std::size_t n = 0; n < newFaces.size(); ++n) {
        const std::vector<VertexHandle>& loop = newFaces[n];
        const u32 count = static_cast<u32>(loop.size());
        if (count < 3) return false;

        Face newFace;
        newFace.material = n < oldMaterials.size() ? oldMaterials[n] : shared;
        const FaceHandle face = m_faces.insert(newFace);
        const std::size_t reference = n < oldCorners.size() ? n : referenceFor(loop);
        if (createdFaces) createdFaces->push_back(face);
        std::vector<EdgeHandle> halves;

        for (u32 i = 0; i < count; ++i) {
            Edge edge;
            edge.tip = loop[(i + 1) % count];
            edge.face = face;
            edge.uv = cornerUV(reference, edge.tip);

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

    // 4. Pair every new half-edge: with another new one, with the outside, or (allowBorders) with a new border half-edge.
    std::vector<u64> usedOutside;
    bool bordersChanged = false;

    for (const auto& [key, handle] : created) {
        const u64 reverse = (key << 32) | (key >> 32);
        const auto oldMark = oldMarks.find(key);
        if (oldMark != oldMarks.end()) m_edges.get(handle).mark = oldMark->second;
        if (oldSeams.contains(key) || oldSeams.contains(reverse)) m_edges.get(handle).seam = true;

        auto twin = created.find(reverse);
        if (twin != created.end()) {
            m_edges.get(handle).pair = twin->second;
            continue;
        }

        auto outer = outside.find(key);
        if (outer != outside.end()) {
            m_edges.get(handle).pair = outer->second;
            m_edges.get(outer->second).pair = handle;
            // The outside half was never removed, so it still holds the edge's mark
            m_edges.get(handle).mark = m_edges.get(outer->second).mark;
            m_edges.get(handle).seam = m_edges.get(outer->second).seam;
            usedOutside.push_back(key);
            continue;
        }

        if (!allowBorders) return false;

        Edge border;
        border.tip = m_edges.get(m_edges.get(handle).prev).tip;
        border.pair = handle;
        border.mark = m_edges.get(handle).mark;
        border.seam = m_edges.get(handle).seam;

        m_edges.get(handle).pair = m_edges.insert(border);
        bordersChanged = true;
    }

    // 5. Outside half-edges nothing pairs with anymore: border edges go away, anything else is an error.
    for (const auto& [key, edge] : outside) {
        if (std::find(usedOutside.begin(), usedOutside.end(), key) != usedOutside.end()) continue;
        if (!allowBorders || !isBorder(edge)) return false;

        m_edges.remove(edge);
        bordersChanged = true;
    }

    if (bordersChanged) relinkBorders();
    return true;
}

void MeshData::relinkBorders() {
    std::map<u32, EdgeHandle> leaving;
    std::vector<EdgeHandle> borders;

    for (EdgeHandle edge : m_edges.getActiveHandles()) {
        if (!isBorder(edge)) continue;
        leaving[getEdgeOrigin(edge).index] = edge;
        borders.push_back(edge);
    }

    for (EdgeHandle edge : borders) {
        const auto next = leaving.find(m_edges.get(edge).tip.index);
        if (next != leaving.end()) link(edge, next->second);
    }
}
