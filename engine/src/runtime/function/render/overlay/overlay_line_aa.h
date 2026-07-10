#pragma once

#include <glm/vec2.hpp>

namespace Blunder {

// Blender overlay_shader_shared.hh: DISC_RADIUS = M_1_SQRTPI * 1.05f
constexpr float kDiscRadius = 0.5936f;
constexpr float kLineSmoothStart = 0.5f - kDiscRadius;
constexpr float kLineSmoothEnd = 0.5f + kDiscRadius;

struct LineData {
  glm::vec2 dir{};
  float dist = 0.0f;
  float dist_raw = 0.0f;
  bool is_valid() const { return dist_raw != 0.0f; }
};

/// Mirrors overlay_antialiasing.bsl.hh Line::decode — packed rg from line MRT.
LineData decodeLineData(glm::vec2 packed);

/// Mirrors line_coverage() — distance_to_line in pixels, line_kernel_size inner full-coverage half-width.
float lineCoverage(float distance_to_line, float line_kernel_size, bool smooth);

/// Mirrors neighbor_dist() — offset is one of (±1,0) or (0,±1).
float neighborLineDist(const LineData& neighbor, glm::ivec2 offset);

}  // namespace Blunder
