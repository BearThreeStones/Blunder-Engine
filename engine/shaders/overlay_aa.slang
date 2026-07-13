// Overlay line anti-aliasing composite — port of Blender overlay_antialiasing.bsl.hh

static const float kDiscRadius = 0.5936;
static const float kLineSmoothStart = 0.5 - kDiscRadius;
static const float kLineSmoothEnd = 0.5 + kDiscRadius;

struct OverlayAaUniformData {
    float4 screenParams; // xy = 1/size, zw = viewport width/height
    float do_smooth_lines;
    float line_kernel;
    float2 _pad;
};

[[vk::binding(0, 0)]]
ConstantBuffer<OverlayAaUniformData> ubo;

[[vk::binding(1, 0)]]
Texture2D<float4> sceneColorTexture;

[[vk::binding(2, 0)]]
SamplerState sceneColorSampler;

[[vk::binding(3, 0)]]
Texture2D<float4> overlayColorTexture;

[[vk::binding(4, 0)]]
SamplerState overlayColorSampler;

[[vk::binding(5, 0)]]
Texture2D<float4> lineDataTexture;

[[vk::binding(6, 0)]]
SamplerState lineDataSampler;

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

void neighborBlend(float4 neighborColor, inout float4 targetColor, float coverage) {
    float4 n = neighborColor * coverage;
    targetColor = targetColor + n * (1.0 - targetColor.a);
}

struct VertexOutput {
    float4 sv_position : SV_Position;
    [[vk::location(0)]]
    float2 uv;
};

[shader("vertex")]
VertexOutput vertexMain(uint vertexId : SV_VertexID) {
    VertexOutput output;
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    output.sv_position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    output.uv = uv * 0.5;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    float2 uv = input.uv;
    float4 scene = sceneColorTexture.Sample(sceneColorSampler, uv);

    int2 texel = int2(uv * ubo.screenParams.zw);
    float4 centerColor = overlayColorTexture.Load(int3(texel, 0));
    LineData centerLine = decodeLineData(lineDataTexture.Load(int3(texel, 0)).rg);

    bool smooth = ubo.do_smooth_lines > 0.5;
    float line_kernel = ubo.line_kernel;

    if (!smooth && centerLine.dist <= 1.0) {
        float3 rgb = lerp(scene.rgb, centerColor.rgb, centerColor.a);
        return float4(rgb, scene.a);
    }

    if (!lineDataValid(centerLine) && centerColor.a < 0.001) {
        float3 rgb = lerp(scene.rgb, centerColor.rgb, centerColor.a);
        return float4(rgb, scene.a);
    }

    float4 neighbors[4];
    neighbors[0] = overlayColorTexture.Load(int3(texel + int2(1, 0), 0));
    neighbors[1] = overlayColorTexture.Load(int3(texel + int2(-1, 0), 0));
    neighbors[2] = overlayColorTexture.Load(int3(texel + int2(0, 1), 0));
    neighbors[3] = overlayColorTexture.Load(int3(texel + int2(0, -1), 0));

    LineData neighborLines[4];
    neighborLines[0] = decodeLineData(lineDataTexture.Load(int3(texel + int2(1, 0), 0)).rg);
    neighborLines[1] = decodeLineData(lineDataTexture.Load(int3(texel + int2(-1, 0), 0)).rg);
    neighborLines[2] = decodeLineData(lineDataTexture.Load(int3(texel + int2(0, 1), 0)).rg);
    neighborLines[3] = decodeLineData(lineDataTexture.Load(int3(texel + int2(0, -1), 0)).rg);

    float4 neighbor_dists = float4(
        neighborLineDist(neighborLines[0], int2(1, 0)),
        neighborLineDist(neighborLines[1], int2(-1, 0)),
        neighborLineDist(neighborLines[2], int2(0, 1)),
        neighborLineDist(neighborLines[3], int2(0, -1)));

    float4 coverage = float4(
        lineCoverageScalar(neighbor_dists.x, line_kernel, smooth),
        lineCoverageScalar(neighbor_dists.y, line_kernel, smooth),
        lineCoverageScalar(neighbor_dists.z, line_kernel, smooth),
        lineCoverageScalar(neighbor_dists.w, line_kernel, smooth));

    if (lineDataValid(centerLine)) {
        float center_cov = lineCoverageScalar(centerLine.dist, line_kernel, smooth);
        centerColor *= center_cov;
    }

    neighborBlend(neighbors[0], centerColor, coverage.x);
    neighborBlend(neighbors[1], centerColor, coverage.y);
    neighborBlend(neighbors[2], centerColor, coverage.z);
    neighborBlend(neighbors[3], centerColor, coverage.w);

    float3 rgb = lerp(scene.rgb, centerColor.rgb, centerColor.a);
    return float4(rgb, scene.a);
}
