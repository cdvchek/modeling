#include "scene/mesh/uv_unwrap.hpp"
#include "core/math/vec4.hpp"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <numeric>
#include <unordered_map>

namespace UVUnwrap {
    namespace {
        constexpr f32 TWO_PI = 6.28318531f;
        constexpr u32 MAX_SOLVER_STEPS = 4000;

        Vec3 toWorldPoint(const Mat4& toWorld, const Vec3& p) {
            const Vec4 w = toWorld * Vec4(p.x, p.y, p.z, 1.0f);
            return Vec3(w.x, w.y, w.z);
        }

        // The face's normal from its corners in the world (Newell's method), unit length or zero
        Vec3 worldNormal(const MeshData& mesh, FaceHandle face, const Mat4& toWorld) {
            const std::vector<VertexHandle> corners = mesh.getFaceVertices(face);
            Vec3 normal(0.0f);
            for (std::size_t i = 0; i < corners.size(); ++i) {
                const Vec3 a = toWorldPoint(toWorld, mesh.getVertexPosition(corners[i]));
                const Vec3 b = toWorldPoint(toWorld, mesh.getVertexPosition(corners[(i + 1) % corners.size()]));
                normal.x += (a.y - b.y) * (a.z + b.z);
                normal.y += (a.z - b.z) * (a.x + b.x);
                normal.z += (a.x - b.x) * (a.y + b.y);
            }
            const f32 length = normal.length();
            return length > 1e-12f ? normal / length : Vec3(0.0f);
        }

        // Seen from outside: the axis direction a face looks along most, and the right and up to lay it out with,
        // the same way the presets are laid out (u right, v down, never mirrored)
        void boxFrame(const Vec3& normal, Vec3& right, Vec3& up) {
            const f32 ax = std::fabs(normal.x), ay = std::fabs(normal.y), az = std::fabs(normal.z);
            Vec3 axis;
            if (ax >= ay && ax >= az) axis = Vec3(normal.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
            else if (ay >= az) axis = Vec3(0.0f, normal.y >= 0.0f ? 1.0f : -1.0f, 0.0f);
            else axis = Vec3(0.0f, 0.0f, normal.z >= 0.0f ? 1.0f : -1.0f);

            // Tops are seen with the back at the top of the picture, bottoms with the front
            if (axis.y > 0.5f) up = Vec3(0.0f, 0.0f, -1.0f);
            else if (axis.y < -0.5f) up = Vec3(0.0f, 0.0f, 1.0f);
            else up = Vec3(0.0f, 1.0f, 0.0f);
            right = Vec3::cross(-axis, up);
        }

        f32 signedArea(Vec2 a, Vec2 b, Vec2 c) {
            return ((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y)) * 0.5f;
        }

        // A sparse least-squares system: rows of (column, value), solved by conjugate gradients on the normal
        // equations (CGLS), so the matrix is never formed
        struct LeastSquares {
            std::vector<std::vector<std::pair<u32, f32>>> rows;
            std::vector<f32> rhs;
            u32 columns = 0;

            void multiply(const std::vector<f32>& x, std::vector<f32>& out) const {
                out.assign(rows.size(), 0.0f);
                for (std::size_t r = 0; r < rows.size(); ++r) {
                    f32 sum = 0.0f;
                    for (const auto& [column, value] : rows[r]) sum += value * x[column];
                    out[r] = sum;
                }
            }

            void multiplyTransposed(const std::vector<f32>& y, std::vector<f32>& out) const {
                out.assign(columns, 0.0f);
                for (std::size_t r = 0; r < rows.size(); ++r) {
                    for (const auto& [column, value] : rows[r]) out[column] += value * y[r];
                }
            }

            void solve(std::vector<f32>& x) const {
                std::vector<f32> ax, r(rows.size()), s, p, q;
                multiply(x, ax);
                for (std::size_t i = 0; i < r.size(); ++i) r[i] = rhs[i] - ax[i];
                multiplyTransposed(r, s);
                p = s;
                const auto dot = [](const std::vector<f32>& a, const std::vector<f32>& b) {
                    f64 sum = 0.0;
                    for (std::size_t i = 0; i < a.size(); ++i) sum += static_cast<f64>(a[i]) * b[i];
                    return sum;
                };
                f64 gamma = dot(s, s);
                const f64 tolerance = std::max(gamma * 1e-14, 1e-24);

                for (u32 step = 0; step < MAX_SOLVER_STEPS && gamma > tolerance; ++step) {
                    multiply(p, q);
                    const f64 qq = dot(q, q);
                    if (qq <= 0.0) break;
                    const f32 alpha = static_cast<f32>(gamma / qq);
                    for (std::size_t i = 0; i < x.size(); ++i) x[i] += alpha * p[i];
                    for (std::size_t i = 0; i < r.size(); ++i) r[i] -= alpha * q[i];
                    multiplyTransposed(r, s);
                    const f64 next = dot(s, s);
                    const f32 beta = static_cast<f32>(next / gamma);
                    for (std::size_t i = 0; i < p.size(); ++i) p[i] = s[i] + beta * p[i];
                    gamma = next;
                }
            }
        };

        // One island, flattened: LSCM over its triangles with two points pinned
        void unwrapIsland(MeshData& mesh, const std::vector<FaceHandle>& faces, const Mat4& toWorld) {
            // A vertex can sit in several places in one island (a seam that cuts in without cutting off), so the
            // unknowns are corner groups: a face's corner (the half-edge ending there) joined with its neighbours'
            // across every edge that isn't a seam
            std::unordered_map<u32, u32> parent;
            std::unordered_map<u32, bool> inIsland;
            for (FaceHandle face : faces) inIsland[face.index] = true;
            const auto find = [&](u32 corner) {
                u32 root = corner;
                while (parent[root] != root) root = parent[root];
                while (parent[corner] != root) {
                    const u32 next = parent[corner];
                    parent[corner] = root;
                    corner = next;
                }
                return root;
            };
            const auto join = [&](u32 a, u32 b) { parent[find(a)] = find(b); };

            for (FaceHandle face : faces) {
                for (EdgeHandle edge : mesh.getLoopEdges(mesh.getFace(face)->edge)) parent[edge.index] = edge.index;
            }
            for (FaceHandle face : faces) {
                for (EdgeHandle edge : mesh.getLoopEdges(mesh.getFace(face)->edge)) {
                    if (mesh.isSeam(edge)) continue;
                    const Edge* half = mesh.getEdge(edge);
                    const Edge* pair = mesh.getEdge(half->pair);
                    if (!pair || pair->face.isNull() || !inIsland.contains(pair->face.index)) continue;
                    // This half runs origin to tip; its pair runs back. The corners at each end meet across the edge.
                    join(edge.index, pair->prev.index);
                    join(half->prev.index, half->pair.index);
                }
            }

            // Each group numbered, with its position in the world
            std::unordered_map<u32, u32> local;
            std::vector<Vec3> positions;
            std::vector<std::array<u32, 3>> triangles;
            const auto groupOf = [&](EdgeHandle corner, VertexHandle vertex) {
                auto [it, added] = local.emplace(find(corner.index), static_cast<u32>(positions.size()));
                if (added) positions.push_back(toWorldPoint(toWorld, mesh.getVertexPosition(vertex)));
                return it->second;
            };
            for (FaceHandle face : faces) {
                // The half-edge ending at each of the face's vertices is that corner
                const std::vector<EdgeHandle> edges = mesh.getLoopEdges(mesh.getFace(face)->edge);
                const auto cornerAt = [&](VertexHandle vertex) {
                    for (EdgeHandle edge : edges) if (mesh.getEdgeTip(edge) == vertex) return edge;
                    return edges.front();
                };
                for (const Triangle& triangle : mesh.getFaceTriangles(face)) {
                    triangles.push_back({ groupOf(cornerAt(triangle.v0), triangle.v0), groupOf(cornerAt(triangle.v1), triangle.v1),
                                          groupOf(cornerAt(triangle.v2), triangle.v2) });
                }
            }
            if (positions.size() < 3) return;

            // Pins: the two vertices farthest apart along the island's longest side
            Vec3 low(FLT_MAX), high(-FLT_MAX);
            for (const Vec3& p : positions) {
                low = Vec3(std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z));
                high = Vec3(std::max(high.x, p.x), std::max(high.y, p.y), std::max(high.z, p.z));
            }
            const Vec3 extent = high - low;
            const u32 axis = extent.x >= extent.y && extent.x >= extent.z ? 0 : extent.y >= extent.z ? 1 : 2;
            const auto along = [axis](const Vec3& p) { return axis == 0 ? p.x : axis == 1 ? p.y : p.z; };
            u32 pinA = 0, pinB = 0;
            for (u32 i = 0; i < positions.size(); ++i) {
                if (along(positions[i]) < along(positions[pinA])) pinA = i;
                if (along(positions[i]) > along(positions[pinB])) pinB = i;
            }
            if (pinA == pinB) return;
            const Vec2 pinnedA(0.0f, 0.0f);
            const Vec2 pinnedB((positions[pinB] - positions[pinA]).length(), 0.0f);

            // Unknowns: u and v of every vertex but the pins
            std::vector<i32> column(positions.size(), -1);
            u32 columns = 0;
            for (u32 i = 0; i < positions.size(); ++i) {
                if (i != pinA && i != pinB) {
                    column[i] = static_cast<i32>(columns);
                    columns += 2;
                }
            }

            // Two rows per triangle: the real and imaginary parts of how far the map is from a similarity there
            LeastSquares system;
            system.columns = columns;
            Vec3 normalSum(0.0f);
            for (const auto& corners : triangles) {
                const Vec3& p0 = positions[corners[0]];
                const Vec3& p1 = positions[corners[1]];
                const Vec3& p2 = positions[corners[2]];
                const Vec3 cross = Vec3::cross(p1 - p0, p2 - p0);
                const f32 doubleArea = cross.length();
                if (doubleArea < 1e-12f) continue;
                normalSum = normalSum + cross;

                // The triangle in its own plane: p0 at the origin, p1 along x
                const Vec3 x = (p1 - p0).normalized();
                const Vec3 y = Vec3::cross(cross / doubleArea, x);
                const Vec2 q[3] = { Vec2(0.0f, 0.0f), Vec2((p1 - p0).length(), 0.0f), Vec2(Vec3::dot(p2 - p0, x), Vec3::dot(p2 - p0, y)) };
                const f32 weight = 1.0f / std::sqrt(doubleArea);

                std::vector<std::pair<u32, f32>> real, imaginary;
                f32 realRhs = 0.0f, imaginaryRhs = 0.0f;
                for (u32 j = 0; j < 3; ++j) {
                    const Vec2 w = q[(j + 2) % 3] - q[(j + 1) % 3];
                    const f32 a = w.x * weight, b = w.y * weight;
                    const u32 vertex = corners[j];
                    if (column[vertex] < 0) {
                        const Vec2 pinned = vertex == pinA ? pinnedA : pinnedB;
                        realRhs -= a * pinned.x - b * pinned.y;
                        imaginaryRhs -= b * pinned.x + a * pinned.y;
                    } else {
                        const u32 c = static_cast<u32>(column[vertex]);
                        real.push_back({ c, a });
                        real.push_back({ c + 1, -b });
                        imaginary.push_back({ c, b });
                        imaginary.push_back({ c + 1, a });
                    }
                }
                system.rows.push_back(std::move(real));
                system.rhs.push_back(realRhs);
                system.rows.push_back(std::move(imaginary));
                system.rhs.push_back(imaginaryRhs);
            }

            // Start from the island laid flat on its average plane, which is usually close
            const Vec3 axisU = (positions[pinB] - positions[pinA]).normalized();
            const Vec3 axisV = Vec3::cross(normalSum.length() > 0.0f ? normalSum.normalized() : Vec3(0.0f, 1.0f, 0.0f), axisU);
            std::vector<f32> solution(columns, 0.0f);
            for (u32 i = 0; i < positions.size(); ++i) {
                if (column[i] < 0) continue;
                const Vec3 d = positions[i] - positions[pinA];
                solution[column[i]] = Vec3::dot(d, axisU);
                solution[column[i] + 1] = Vec3::dot(d, axisV);
            }
            system.solve(solution);

            std::vector<Vec2> uv(positions.size());
            for (u32 i = 0; i < positions.size(); ++i) {
                if (i == pinA) uv[i] = pinnedA;
                else if (i == pinB) uv[i] = pinnedB;
                else uv[i] = Vec2(solution[column[i]], solution[column[i] + 1]);
            }

            // The right way round: the same winding a box projection of the island's largest triangle has
            f32 uvArea = 0.0f, worldArea = 0.0f, largest = 0.0f;
            u32 reference = 0;
            for (u32 t = 0; t < triangles.size(); ++t) {
                const auto& c = triangles[t];
                uvArea += signedArea(uv[c[0]], uv[c[1]], uv[c[2]]);
                const f32 area = Vec3::cross(positions[c[1]] - positions[c[0]], positions[c[2]] - positions[c[0]]).length() * 0.5f;
                worldArea += area;
                if (area > largest) {
                    largest = area;
                    reference = t;
                }
            }
            const auto& r = triangles[reference];
            Vec3 right, up;
            boxFrame(Vec3::cross(positions[r[1]] - positions[r[0]], positions[r[2]] - positions[r[0]]).normalized(), right, up);
            const auto boxUV = [&](u32 v) { return Vec2(Vec3::dot(positions[v], right), -Vec3::dot(positions[v], up)); };
            const bool wanted = signedArea(boxUV(r[0]), boxUV(r[1]), boxUV(r[2])) >= 0.0f;
            if ((uvArea >= 0.0f) != wanted) {
                for (Vec2& p : uv) p.y = -p.y;
            }

            // Square to the texture: turned to whichever of its own edge directions gives the smallest bounds, so a
            // square face sits square rather than as a diamond
            f32 bestAngle = 0.0f, bestArea = FLT_MAX;
            for (const auto& c : triangles) {
                for (u32 k = 0; k < 3; ++k) {
                    const Vec2 d = uv[c[(k + 1) % 3]] - uv[c[k]];
                    if (d.length() < 1e-9f) continue;
                    const f32 angle = -std::atan2(d.y, d.x);
                    const f32 cs = std::cos(angle), sn = std::sin(angle);
                    Vec2 lo(FLT_MAX, FLT_MAX), hi(-FLT_MAX, -FLT_MAX);
                    for (const Vec2& p : uv) {
                        const Vec2 t(p.x * cs - p.y * sn, p.x * sn + p.y * cs);
                        lo = Vec2(std::min(lo.x, t.x), std::min(lo.y, t.y));
                        hi = Vec2(std::max(hi.x, t.x), std::max(hi.y, t.y));
                    }
                    const f32 area = (hi.x - lo.x) * (hi.y - lo.y);
                    if (area < bestArea - 1e-9f) {
                        bestArea = area;
                        bestAngle = angle;
                    }
                }
            }
            const f32 cs = std::cos(bestAngle), sn = std::sin(bestAngle);
            for (Vec2& p : uv) p = Vec2(p.x * cs - p.y * sn, p.x * sn + p.y * cs);

            // Its real size: UV area matches world area, so every island gets detail in proportion
            const f32 scale = std::fabs(uvArea) > 1e-12f ? std::sqrt(worldArea / std::fabs(uvArea)) : 1.0f;
            for (FaceHandle face : faces) {
                const std::vector<EdgeHandle> edges = mesh.getLoopEdges(mesh.getFace(face)->edge);
                std::vector<Vec2> uvs;
                for (EdgeHandle edge : edges) {
                    const auto found = local.find(find(edge.index));
                    uvs.push_back(found != local.end() ? uv[found->second] * scale : Vec2(0.0f, 0.0f));
                }
                mesh.setFaceUVs(face, uvs);
            }
        }

        // Fixes faces that cross the seam of a wrapped projection: corners far to one side move round by a full turn
        void wrapFace(std::vector<Vec2>& uvs) {
            f32 low = FLT_MAX, high = -FLT_MAX;
            for (const Vec2& uv : uvs) {
                low = std::min(low, uv.x);
                high = std::max(high, uv.x);
            }
            if (high - low <= 0.5f) return;
            for (Vec2& uv : uvs) if (uv.x < 0.5f) uv.x += 1.0f;
        }

        // The faces' vertices' middle and height in the mesh's own space
        void spread(const MeshData& mesh, const std::vector<FaceHandle>& faces, Vec3& low, Vec3& high) {
            low = Vec3(FLT_MAX);
            high = Vec3(-FLT_MAX);
            for (FaceHandle face : faces) {
                for (VertexHandle vertex : mesh.getFaceVertices(face)) {
                    const Vec3 p = mesh.getVertexPosition(vertex);
                    low = Vec3(std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z));
                    high = Vec3(std::max(high.x, p.x), std::max(high.y, p.y), std::max(high.z, p.z));
                }
            }
        }
    }

    std::vector<std::vector<FaceHandle>> seamIslands(const MeshData& mesh, const std::vector<FaceHandle>& faces) {
        std::unordered_map<u32, bool> inList;
        for (FaceHandle face : faces) inList[face.index] = false;

        std::vector<std::vector<FaceHandle>> islands;
        for (FaceHandle start : faces) {
            if (inList[start.index]) continue;
            inList[start.index] = true;

            std::vector<FaceHandle> island;
            std::vector<FaceHandle> pending { start };
            while (!pending.empty()) {
                const FaceHandle face = pending.back();
                pending.pop_back();
                island.push_back(face);

                const Face* data = mesh.getFace(face);
                if (!data) continue;
                for (EdgeHandle edge : mesh.getLoopEdges(data->edge)) {
                    if (mesh.isSeam(edge)) continue;
                    const Edge* half = mesh.getEdge(edge);
                    const Edge* pair = half ? mesh.getEdge(half->pair) : nullptr;
                    if (!pair || pair->face.isNull()) continue;
                    const auto next = inList.find(pair->face.index);
                    if (next == inList.end() || next->second) continue;
                    next->second = true;
                    pending.push_back(pair->face);
                }
            }
            islands.push_back(std::move(island));
        }
        return islands;
    }

    std::vector<std::vector<FaceHandle>> uvIslands(const MeshData& mesh, const std::vector<FaceHandle>& faces) {
        std::unordered_map<u32, bool> inList;
        for (FaceHandle face : faces) inList[face.index] = false;

        std::vector<std::vector<FaceHandle>> islands;
        for (FaceHandle start : faces) {
            if (inList[start.index]) continue;
            std::vector<FaceHandle> island;
            for (FaceHandle face : mesh.getUVIsland(start)) {
                const auto found = inList.find(face.index);
                if (found == inList.end() || found->second) continue;
                found->second = true;
                island.push_back(face);
            }
            if (!island.empty()) islands.push_back(std::move(island));
        }
        return islands;
    }

    std::vector<std::vector<FaceHandle>> unwrap(MeshData& mesh, const std::vector<FaceHandle>& faces, const Mat4& toWorld) {
        std::vector<std::vector<FaceHandle>> islands = seamIslands(mesh, faces);
        for (const std::vector<FaceHandle>& island : islands) unwrapIsland(mesh, island, toWorld);
        return islands;
    }

    void projectPlanar(MeshData& mesh, const std::vector<FaceHandle>& faces, const Mat4& toWorld, const Vec3& right, const Vec3& up) {
        for (FaceHandle face : faces) {
            std::vector<Vec2> uvs;
            for (VertexHandle vertex : mesh.getFaceVertices(face)) {
                const Vec3 p = toWorldPoint(toWorld, mesh.getVertexPosition(vertex));
                uvs.push_back(Vec2(Vec3::dot(p, right), -Vec3::dot(p, up)));
            }
            mesh.setFaceUVs(face, uvs);
        }
    }

    void projectBox(MeshData& mesh, const std::vector<FaceHandle>& faces, const Mat4& toWorld) {
        for (FaceHandle face : faces) {
            Vec3 right, up;
            boxFrame(worldNormal(mesh, face, toWorld), right, up);
            std::vector<Vec2> uvs;
            for (VertexHandle vertex : mesh.getFaceVertices(face)) {
                const Vec3 p = toWorldPoint(toWorld, mesh.getVertexPosition(vertex));
                uvs.push_back(Vec2(Vec3::dot(p, right), -Vec3::dot(p, up)));
            }
            mesh.setFaceUVs(face, uvs);
        }
    }

    void projectCylinder(MeshData& mesh, const std::vector<FaceHandle>& faces) {
        Vec3 low, high;
        spread(mesh, faces, low, high);
        const Vec3 center = (low + high) * 0.5f;

        // Around stays in proportion to height: a full turn is the circumference
        f32 radius = 0.0f;
        u32 count = 0;
        for (FaceHandle face : faces) {
            for (VertexHandle vertex : mesh.getFaceVertices(face)) {
                const Vec3 p = mesh.getVertexPosition(vertex) - center;
                radius += std::sqrt(p.x * p.x + p.z * p.z);
                ++count;
            }
        }
        radius = count > 0 ? std::max(radius / count, 1e-4f) : 1.0f;

        for (FaceHandle face : faces) {
            std::vector<Vec2> uvs;
            for (VertexHandle vertex : mesh.getFaceVertices(face)) {
                const Vec3 p = mesh.getVertexPosition(vertex) - center;
                uvs.push_back(Vec2(std::atan2(p.x, p.z) / TWO_PI + 0.5f, (high.y - center.y - p.y) / (TWO_PI * radius)));
            }
            wrapFace(uvs);
            mesh.setFaceUVs(face, uvs);
        }
    }

    void projectSphere(MeshData& mesh, const std::vector<FaceHandle>& faces) {
        Vec3 low, high;
        spread(mesh, faces, low, high);
        const Vec3 center = (low + high) * 0.5f;

        // Pole to pole is half a turn, so v covers half what u does
        for (FaceHandle face : faces) {
            std::vector<Vec2> uvs;
            for (VertexHandle vertex : mesh.getFaceVertices(face)) {
                const Vec3 d = mesh.getVertexPosition(vertex) - center;
                const f32 length = std::max(d.length(), 1e-6f);
                uvs.push_back(Vec2(std::atan2(d.x, d.z) / TWO_PI + 0.5f, std::acos(std::clamp(d.y / length, -1.0f, 1.0f)) / TWO_PI));
            }
            wrapFace(uvs);
            mesh.setFaceUVs(face, uvs);
        }
    }

    bool bounds(const MeshData& mesh, const std::vector<FaceHandle>& faces, Area& out) {
        out.low = Vec2(FLT_MAX, FLT_MAX);
        out.high = Vec2(-FLT_MAX, -FLT_MAX);
        for (FaceHandle face : faces) {
            for (const Vec2& uv : mesh.getFaceUVs(face)) {
                out.low = Vec2(std::min(out.low.x, uv.x), std::min(out.low.y, uv.y));
                out.high = Vec2(std::max(out.high.x, uv.x), std::max(out.high.y, uv.y));
            }
        }
        return out.low.x <= out.high.x;
    }

    void pack(MeshData& mesh, const std::vector<std::vector<FaceHandle>>& islands, const Area& area, f32 margin, bool rotate) {
        struct Piece {
            const std::vector<FaceHandle>* faces;
            bool turned = false;
            Vec2 low, size;
            Vec2 place;
        };

        // Turning a quarter turn: (u, v) to (v, -u), a rotation, never a mirror
        const auto turn = [](const Vec2& uv) { return Vec2(uv.y, -uv.x); };

        std::vector<Piece> pieces;
        for (const std::vector<FaceHandle>& faces : islands) {
            Area box;
            if (!bounds(mesh, faces, box)) continue;
            Piece piece { &faces };
            Vec2 size = box.high - box.low;
            if (rotate && size.y > size.x) {
                piece.turned = true;
                Vec2 low(FLT_MAX, FLT_MAX), high(-FLT_MAX, -FLT_MAX);
                for (FaceHandle face : faces) {
                    for (const Vec2& uv : mesh.getFaceUVs(face)) {
                        const Vec2 t = turn(uv);
                        low = Vec2(std::min(low.x, t.x), std::min(low.y, t.y));
                        high = Vec2(std::max(high.x, t.x), std::max(high.y, t.y));
                    }
                }
                box.low = low;
                box.high = high;
                size = high - low;
            }
            piece.low = box.low;
            piece.size = size;
            pieces.push_back(piece);
        }
        if (pieces.empty()) return;

        // Tallest first, in rows across the area
        std::vector<u32> order(pieces.size());
        std::iota(order.begin(), order.end(), 0u);
        std::stable_sort(order.begin(), order.end(), [&](u32 a, u32 b) { return pieces[a].size.y > pieces[b].size.y; });

        const f32 width = area.high.x - area.low.x;
        const f32 height = area.high.y - area.low.y;
        const f32 gap = margin * width;
        const auto fits = [&](f32 scale, bool record) {
            f32 x = gap, y = gap, row = 0.0f;
            for (u32 index : order) {
                Piece& piece = pieces[index];
                const f32 w = piece.size.x * scale, h = piece.size.y * scale;
                if (w + gap * 2.0f > width) return false;
                if (x + w + gap > width) {
                    x = gap;
                    y += row + gap;
                    row = 0.0f;
                }
                if (record) piece.place = Vec2(x, y);
                x += w + gap;
                row = std::max(row, h);
            }
            return y + row + gap <= height;
        };

        // The largest scale that fits, found by halving the range
        f32 largest = 0.0f;
        for (const Piece& piece : pieces) largest = std::max({ largest, piece.size.x, piece.size.y });
        if (largest <= 0.0f) return;
        f32 lowScale = 0.0f, highScale = std::min(width, height) / largest;
        for (u32 step = 0; step < 40; ++step) {
            const f32 middle = (lowScale + highScale) * 0.5f;
            if (fits(middle, false)) lowScale = middle;
            else highScale = middle;
        }
        fits(lowScale, true);

        for (const Piece& piece : pieces) {
            for (FaceHandle face : *piece.faces) {
                std::vector<Vec2> uvs = mesh.getFaceUVs(face);
                for (Vec2& uv : uvs) {
                    const Vec2 from = piece.turned ? turn(uv) : uv;
                    uv = area.low + piece.place + (from - piece.low) * lowScale;
                }
                mesh.setFaceUVs(face, uvs);
            }
        }
    }
}
