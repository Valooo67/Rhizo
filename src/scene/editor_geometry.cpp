#include "rhizo/editor_geometry.hpp"
#include <algorithm>
#include <cmath>

namespace rhizo {
EditorGeometry make_grid(float half_extent, uint32_t subdivisions) {
  EditorGeometry result;
  if (!std::isfinite(half_extent) || half_extent <= 0 || subdivisions == 0 || subdivisions > 512) return result;
  const uint32_t line_count = subdivisions * 2u + 1u;
  result.vertices.reserve(static_cast<size_t>(line_count) * 4u);
  const float step = (half_extent * 2.0f) / static_cast<float>(subdivisions);
  for (uint32_t i = 0; i < line_count; ++i) {
    const float p = -half_extent + step * static_cast<float>(i);
    const bool major = (i % 10u) == 0u;
    const float alpha = major ? 0.75f : 0.28f;
    const Vec4 grid_color{0.48f, 0.53f, 0.60f, alpha};
    result.vertices.push_back({{-half_extent, 0, p}, grid_color});
    result.vertices.push_back({{ half_extent, 0, p}, grid_color});
    result.vertices.push_back({{p, 0, -half_extent}, grid_color});
    result.vertices.push_back({{p, 0,  half_extent}, grid_color});
  }
  // World axes are deliberately emitted last so an editor can draw them on top.
  result.vertices.push_back({{-half_extent, 0, 0}, {0.9f, 0.18f, 0.16f, 1}});
  result.vertices.push_back({{ half_extent, 0, 0}, {0.9f, 0.18f, 0.16f, 1}});
  result.vertices.push_back({{0, 0, -half_extent}, {0.2f, 0.75f, 0.3f, 1}});
  result.vertices.push_back({{0, 0,  half_extent}, {0.2f, 0.75f, 0.3f, 1}});
  return result;
}

EditorGeometry make_transform_gizmo(Vec3 origin, float axis_length) {
  EditorGeometry result;
  if (!std::isfinite(axis_length) || axis_length <= 0) return result;
  const float tip = axis_length;
  const float stem = axis_length * 0.78f;
  const float wing = axis_length * 0.055f;
  result.vertices.reserve(18);
  const Vec4 colors[3] = {{0.95f,0.16f,0.14f,1},{0.2f,0.88f,0.3f,1},{0.18f,0.48f,1,1}};
  const Vec3 dirs[3] = {{1,0,0},{0,1,0},{0,0,1}};
  for (int axis=0; axis<3; ++axis) {
    const Vec3 d=dirs[axis];
    result.vertices.push_back({origin,colors[axis]});
    result.vertices.push_back({origin+d*stem,colors[axis]});
    result.vertices.push_back({origin+d*stem,colors[axis]});
    result.vertices.push_back({origin+d*tip,colors[axis]});
    Vec3 side = axis==0 ? Vec3{0,wing,0} : (axis==1 ? Vec3{wing,0,0} : Vec3{wing,0,0});
    result.vertices.push_back({origin+d*stem+side,colors[axis]});
    result.vertices.push_back({origin+d*tip,colors[axis]});
  }
  return result;
}
}
