#pragma once

namespace Blunder {

constexpr float kPolylineSmoothWidth = 1.0f;

/// stroke_coord: 0 = center of polyline quad, 1 = outer edge; fw = filter width (~fwidth).
float polylineStrokeAlpha(float stroke_coord, float fw);

/// Blender gpu_shader_3D_polyline_frag: smoothline in pixels, line_width in pixels.
float polylineStrokeAlphaBlender(float smoothline, float line_width, bool line_smooth);

}  // namespace Blunder
