#pragma once

namespace Blunder {

/// Signed distance from point (px,py) to segment [a,b] in 2D.
float sdSegment2d(float px, float py, float ax, float ay, float bx, float by);

/// Filled disc AA: signed_px_dist = (r - radius) / units_per_pixel.
float discFillAlpha(float signed_px_dist, float edge_px = 1.25f);

/// Line segment stroke AA in pixel space (mirrors shader segmentLineAlpha).
float segmentLineAlpha(float px, float py, float ax, float ay, float bx, float by,
                       float half_width_px, float units_per_pixel);

/// Signed distance to axis-aligned box centered at origin (negative inside).
float sdBox2d(float px, float py, float half_w, float half_h);

/// Rectangle border stroke AA in pixel space (mirrors shader rectRingAlpha).
float rectRingAlpha(float px, float py, float half_w, float half_h, float half_width_px,
                    float units_per_pixel);

/// Filled rectangle interior AA (mirrors shader rectFillAlpha).
float rectFillAlpha(float px, float py, float half_w, float half_h, float units_per_pixel,
                    float edge_px = 0.75f);

}  // namespace Blunder
