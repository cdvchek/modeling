#pragma once

#include "core/containers/dynamic_array.hpp"

struct VertexTag {};
struct EdgeTag {};
struct FaceTag {};

using VertexHandle = Handle<VertexTag>;
using EdgeHandle   = Handle<EdgeTag>;
using FaceHandle   = Handle<FaceTag>;

constexpr VertexHandle INVALID_VERTEX { INVALID_INDEX, 0 };
constexpr EdgeHandle INVALID_EDGE { INVALID_INDEX, 0 };
constexpr FaceHandle INVALID_FACE { INVALID_INDEX, 0 };
