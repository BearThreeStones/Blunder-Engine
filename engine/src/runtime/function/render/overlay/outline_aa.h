#pragma once

namespace Blunder {

constexpr float kOutlineEdgeSmoothMin = 0.25f;
constexpr float kOutlineEdgeSmoothMax = 2.5f;

/// Mirrors outline_resolve.slang edgeCoverage() - neighbor_edge_count in [0,8].
float outlineEdgeCoverage(int neighbor_edge_count);

}  // namespace Blunder
