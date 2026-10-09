#pragma once

#include <vector>
#include "scene/selection/selection.hpp"
#include "scene/mesh/mesh_data.hpp"

// Selecting mesh elements so the selection stays whole: an edge or face brings its vertices along, and taking one
// away drops only the vertices nothing else selected still uses. The 3D view and the UV editor both use these.

// Either half of the edge counts
bool isEdgeSelected(const Selection& selection, ObjectHandle object, const MeshData& mesh, EdgeHandle edge);
void selectEdge(Selection& selection, ObjectHandle object, const MeshData& mesh, EdgeHandle edge);
void selectFace(Selection& selection, ObjectHandle object, const MeshData& mesh, FaceHandle face);
void deselectEdge(Selection& selection, ObjectHandle object, const MeshData& mesh, EdgeHandle edge);
void deselectFace(Selection& selection, ObjectHandle object, const MeshData& mesh, FaceHandle face);

// Every element of the object for a selection mode: its vertices, its edges (one half each, with their vertices),
// or its faces (with their vertices)
enum class SelectAllMode : u8 { Vertices, Edges, Faces };
void selectAll(Selection& selection, ObjectHandle object, const MeshData& mesh, SelectAllMode mode);
