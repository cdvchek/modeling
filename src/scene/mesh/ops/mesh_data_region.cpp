#include "scene/mesh/mesh_data.hpp"

#include <algorithm>
#include <limits>
#include <map>

namespace {
    // Keeps an inset corner from shooting off when its two edges nearly fold back on each other
    constexpr f32 MIN_COS = 0.1f;
}

const char* regionErrorText(RegionError error) {
    switch (error) {
        case RegionError::None: return "";
        case RegionError::NoFaces: return "no faces selected";
        case RegionError::CornerTouch: return "selected faces touch only at a corner";
        case RegionError::NoBoundary: return "the selection is a closed surface with no edge to build from";
        case RegionError::Holes: return "the selection has a hole in it";
        case RegionError::Failed: return "the mesh couldn't be rebuilt around the selection";
    }
    return "";
}

RegionError MeshData::findRegions(const std::vector<FaceHandle>& faces, std::vector<Region>& regions) const {
    std::vector<FaceHandle> selected;
    for (FaceHandle face : faces) {
        if (isValidHandle(face) && std::find(selected.begin(), selected.end(), face) == selected.end()) selected.push_back(face);
    }
    if (selected.empty()) return RegionError::NoFaces;

    auto indexOf = [&](FaceHandle face) {
        const auto it = std::find(selected.begin(), selected.end(), face);
        return it == selected.end() ? -1 : static_cast<i32>(it - selected.begin());
    };

    auto faceAcross = [&](EdgeHandle edge) {
        const Edge* pair = m_edges.tryGet(m_edges.get(edge).pair);
        return pair ? pair->face : INVALID_FACE;
    };

    // 1. Group faces that share an edge.
    std::vector<i32> regionOf(selected.size(), -1);
    regions.clear();

    for (u32 seed = 0; seed < static_cast<u32>(selected.size()); ++seed) {
        if (regionOf[seed] >= 0) continue;

        const i32 id = static_cast<i32>(regions.size());
        regions.push_back({});
        regionOf[seed] = id;

        std::vector<u32> stack { seed };
        while (!stack.empty()) {
            const u32 current = stack.back();
            stack.pop_back();
            regions[id].faces.push_back(selected[current]);

            for (EdgeHandle edge : getFaceEdges(selected[current])) {
                const i32 neighbor = indexOf(faceAcross(edge));
                if (neighbor >= 0 && regionOf[neighbor] < 0) {
                    regionOf[neighbor] = id;
                    stack.push_back(static_cast<u32>(neighbor));
                }
            }
        }
    }

    // 2. Separate regions may not share a vertex.
    std::map<u32, i32> vertexRegion;
    for (i32 id = 0; id < static_cast<i32>(regions.size()); ++id) {
        for (FaceHandle face : regions[id].faces) {
            for (VertexHandle vertex : getFaceVertices(face)) {
                const auto [it, added] = vertexRegion.emplace(vertex.index, id);
                if (!added && it->second != id) return RegionError::CornerTouch;
            }
        }
    }

    // 3. Each region's boundary must be one simple loop.
    for (i32 id = 0; id < static_cast<i32>(regions.size()); ++id) {
        Region& region = regions[id];
        std::map<u32, EdgeHandle> startingAt;
        u32 boundaryCount = 0;

        for (FaceHandle face : region.faces) {
            for (EdgeHandle edge : getFaceEdges(face)) {
                const i32 across = indexOf(faceAcross(edge));
                if (across >= 0 && regionOf[across] == id) continue;

                // Two boundary edges leaving one vertex: the region pinches there
                if (!startingAt.emplace(getEdgeOrigin(edge).index, edge).second) return RegionError::CornerTouch;
                ++boundaryCount;
            }
        }

        if (boundaryCount == 0) return RegionError::NoBoundary;

        const EdgeHandle first = startingAt.begin()->second;
        EdgeHandle edge = first;

        do {
            region.boundary.push_back(edge);
            const auto next = startingAt.find(getEdgeTip(edge).index);
            if (next == startingAt.end()) return RegionError::CornerTouch;
            edge = next->second;
        } while (edge != first && region.boundary.size() <= boundaryCount);

        if (region.boundary.size() != boundaryCount) return RegionError::Holes;
    }

    return RegionError::None;
}

bool MeshData::ringRegions(
    const std::vector<Region>& regions,
    std::vector<FaceHandle>& topFaces,
    std::vector<VertexHandle>& originals,
    std::vector<VertexHandle>& copies
) {
    std::vector<FaceHandle> oldFaces;
    std::vector<std::vector<VertexHandle>> tops;
    std::vector<std::vector<VertexHandle>> rings;

    for (const Region& region : regions) {
        std::map<u32, VertexHandle> copyOf;

        for (EdgeHandle edge : region.boundary) {
            const VertexHandle original = getEdgeOrigin(edge);
            const VertexHandle copy = addVertex(getVertexPosition(original));

            copyOf[original.index] = copy;
            originals.push_back(original);
            copies.push_back(copy);
        }

        // The region's faces move onto the copies; interior vertices stay theirs
        for (FaceHandle face : region.faces) {
            std::vector<VertexHandle> loop = getFaceVertices(face);
            for (VertexHandle& vertex : loop) {
                const auto copy = copyOf.find(vertex.index);
                if (copy != copyOf.end()) vertex = copy->second;
            }

            oldFaces.push_back(face);
            tops.push_back(loop);
        }

        // A quad between each old boundary edge and its copy
        for (EdgeHandle edge : region.boundary) {
            const VertexHandle a = getEdgeOrigin(edge);
            const VertexHandle b = getEdgeTip(edge);
            rings.push_back({ a, b, copyOf[b.index], copyOf[a.index] });
        }
    }

    std::vector<std::vector<VertexHandle>> newFaces = tops;
    newFaces.insert(newFaces.end(), rings.begin(), rings.end());

    std::vector<FaceHandle> created;
    if (!replaceFaces(oldFaces, newFaces, &created)) return false;

    topFaces.assign(created.begin(), created.begin() + static_cast<std::ptrdiff_t>(tops.size()));
    return true;
}

RegionError MeshData::extrudeRegions(const std::vector<FaceHandle>& faces, std::vector<FaceHandle>& topFaces) {
    std::vector<Region> regions;
    const RegionError error = findRegions(faces, regions);
    if (error != RegionError::None) return error;

    std::vector<VertexHandle> originals;
    std::vector<VertexHandle> copies;
    topFaces.clear();
    return ringRegions(regions, topFaces, originals, copies) ? RegionError::None : RegionError::Failed;
}

RegionError MeshData::insetRegions(const std::vector<FaceHandle>& faces, SlideSession& session, std::vector<FaceHandle>& innerFaces, const Mat4& space) {
    RegionError error = RegionError::None;

    runInSpace(session, space, [&] {
        std::vector<Region> regions;
        error = findRegions(faces, regions);
        if (error != RegionError::None) return false;

        session.savedVertices = m_vertices;
        session.savedEdges = m_edges;
        session.savedFaces = m_faces;

        // 1. Each boundary vertex slides into the region, in the plane of its faces, so both its edges move in by the width.
        std::map<u32, Vec3> slideOf;
        f32 maxWidth = std::numeric_limits<f32>::max();

        for (const Region& region : regions) {
            const u32 count = static_cast<u32>(region.boundary.size());

            for (u32 i = 0; i < count; ++i) {
                const EdgeHandle incoming = region.boundary[(i + count - 1) % count];
                const EdgeHandle outgoing = region.boundary[i];
                const VertexHandle vertex = getEdgeOrigin(outgoing);

                // Left of an edge, looking down its face's normal, is inside the face
                auto inward = [&](EdgeHandle edge) {
                    const Vec3 along = getVertexPosition(getEdgeTip(edge)) - getVertexPosition(getEdgeOrigin(edge));
                    return Vec3::cross(getFaceNormal(m_edges.get(edge).face), along).normalized();
                };

                const Vec3 a = inward(incoming);
                const Vec3 b = inward(outgoing);
                const Vec3 sum = a + b;
                const Vec3 middle = sum.length() > 1e-4f ? sum.normalized() : a;
                const Vec3 slide = middle * (1.0f / std::max(Vec3::dot(middle, a), MIN_COS));
                slideOf[vertex.index] = slide;

                // Stop before the slide reaches halfway along any edge of the region leaving this vertex
                for (FaceHandle face : region.faces) {
                    const std::vector<VertexHandle> loop = getFaceVertices(face);
                    const u32 sides = static_cast<u32>(loop.size());

                    for (u32 k = 0; k < sides; ++k) {
                        if (loop[k] != vertex) continue;

                        for (VertexHandle neighbor : { loop[(k + 1) % sides], loop[(k + sides - 1) % sides] }) {
                            const Vec3 toNeighbor = getVertexPosition(neighbor) - getVertexPosition(vertex);
                            const f32 length = toNeighbor.length();
                            if (length < 1e-6f) continue;

                            const f32 along = Vec3::dot(slide, toNeighbor / length);
                            if (along > 1e-6f) maxWidth = std::min(maxWidth, 0.5f * length / along);
                        }
                    }
                }
            }
        }

        // 2. Build the rings, then hand the copies to the session.
        std::vector<VertexHandle> originals;
        std::vector<VertexHandle> copies;
        innerFaces.clear();
        if (!ringRegions(regions, innerFaces, originals, copies)) {
            cancelSlide(session);
            error = RegionError::Failed;
            return false;
        }

        session.vertices = copies;
        session.starts.clear();
        session.directions.clear();
        for (u32 i = 0; i < static_cast<u32>(copies.size()); ++i) {
            session.starts.push_back(getVertexPosition(copies[i]));
            session.directions.push_back(slideOf[originals[i].index]);
        }
        session.maxWidth = maxWidth;

        return true;
    });

    return error;
}
