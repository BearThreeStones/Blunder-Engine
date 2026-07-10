// Shared line MRT pack/decode — mirrors overlay_common_lib.glsl and overlay_line_aa.cpp

static const float kDiscRadius = 0.5936;
static const float kLineSmoothStart = 0.5 - kDiscRadius;
static const float kLineSmoothEnd = 0.5 + kDiscRadius;

struct LineData {
    float2 dir;
    float dist;
    float dist_raw;
};

LineData decodeLineData(float2 packed) {
    float dist = (packed.y - 0.5) * 2.5;
    float sin_theta = (packed.x - 0.5) * 2.0;
    float cos_theta = sqrt(max(1.0 - sin_theta * sin_theta, 0.0));
    float2 perp = normalize(float2(sin_theta, cos_theta));
    return { perp, dist, packed.y };
}

bool lineDataValid(LineData line) {
    return line.dist_raw != 0.0;
}

float lineCoverageScalar(float distance_to_line, float line_kernel_size, bool smooth) {
    float x = abs(distance_to_line) - line_kernel_size;
    if (smooth) {
        float t = saturate((x - kLineSmoothStart) / max(kLineSmoothEnd - kLineSmoothStart, 1e-6));
        float s = t * t * (3.0 - 2.0 * t);
        return 1.0 - s;
    }
    return step(-0.5, line_kernel_size - abs(distance_to_line));
}

float neighborLineDist(LineData neighbor, int2 offset) {
    bool is_dir_horizontal = abs(neighbor.dir.x) > abs(neighbor.dir.y);
    bool is_ofs_horizontal = offset.x != 0;
    if (!lineDataValid(neighbor) || is_ofs_horizontal != is_dir_horizontal) {
        return 1e10;
    }
    return neighbor.dist + dot(float2(offset), -neighbor.dir);
}

/// edge_start and edge_pos in pixel coordinates (same space as frag_coord).
float4 packLineData(float2 frag_co, float2 edge_start, float2 edge_pos) {
    float2 edge = edge_start - edge_pos;
    float len = length(edge);
    if (len > 0.0) {
        edge /= len;
        float2 perp = float2(-edge.y, edge.x);
        if (perp.y < 0.0) {
            perp = -perp;
        }
        float sin_theta = perp.x;
        float dist = dot(perp, frag_co - edge_start);
        return float4(sin_theta * 0.5 + 0.5, dist * 0.4 + 0.5, 0.0, 1.0);
    }
    return float4(0.0, 0.5, 0.0, 1.0);
}
