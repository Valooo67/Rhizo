#pragma once
#include "rhizo/math.hpp"
#include <cstdint>
#include <vector>

namespace rhizo {
struct EditorVertex { Vec3 position; Vec4 color; };
struct EditorGeometry { std::vector<EditorVertex> vertices; };
EditorGeometry make_grid(float half_extent, uint32_t subdivisions);
EditorGeometry make_transform_gizmo(Vec3 origin, float axis_length);
}
