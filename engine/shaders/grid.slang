// grid.slang — Analytical anti-aliased ground-plane grid with GridPass LOD.
//
// Renders as a large quad on the grid plane using TriangleList topology
// (6 vertices from SV_VertexID).  Grid lines are computed analytically
// per pixel in the fragment shader using fwidth() for perfect AA at any
// zoom level.
//
// Nested LOD (GridPass): three exponential spacings from log10(viewDist),
// cross-faded with fracLog weights (1-f, 1, f). Horizon fade uses
// |ray · planeNormal| so the grid dissolves toward the horizon.

static const uint GRID_STEPS_LEN  = 8u;
static const uint GRID_STEPS_DRAW = 3u;

struct GridUniformData
{
    float4x4 view;
    float4x4 projection;
    float4 cameraPositionAndProjType;  // xyz=pos, w=isOrtho
    float4 cameraForwardAndFarClip;    // xyz=forward, w=farClip
    float4 quadCenter;                 // xyz = camera projection onto grid plane
    float4 planeAxisU;                 // xyz = first grid axis; w = worldPerPixel
    float4 planeAxisV;                 // xyz = second grid axis
    float4 steps[GRID_STEPS_LEN];      // [0..2] = nested LOD spacings
    // params: x=fracLog, y=quadHalfExtent, z=alphaScale, w=unused
    float4 params;
    float4 axisColorU;                 // color+alpha for axis along U (at v=0, u>0)
    float4 axisColorV;                 // color+alpha for axis along V (at u=0, v>0)
};

[[vk::binding(0, 0)]]
ConstantBuffer<GridUniformData> ubo;

struct VertexOutput
{
    float4 sv_position : SV_Position;
    [[vk::location(0)]]
    float3 worldPos;
    [[vk::location(1)]]
    float2 planeUV;
};

[shader("vertex")]
VertexOutput vertexMain(uint vertexId : SV_VertexID)
{
    VertexOutput output;

    float extent = ubo.params.y;

    float2 corners[6] = {
        float2(-1, -1), float2( 1, -1), float2( 1,  1),
        float2(-1, -1), float2( 1,  1), float2(-1,  1)
    };
    float2 corner = corners[vertexId] * extent;

    float3 center = ubo.quadCenter.xyz;
    float3 axisU = ubo.planeAxisU.xyz;
    float3 axisV = ubo.planeAxisV.xyz;

    float3 planeNormal = normalize(cross(axisU, axisV));
    float3 world = center + axisU * corner.x + axisV * corner.y
                   + planeNormal * 0.02;

    output.sv_position = mul(ubo.projection, mul(ubo.view, float4(world, 1.0)));
    output.worldPos = world;
    output.planeUV = float2(dot(world, axisU), dot(world, axisV));
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    float u = input.planeUV.x;
    float v = input.planeUV.y;

    float alphaScale = ubo.params.z;
    float fracLog = saturate(ubo.params.x);

    float du = fwidth(u);
    float dv = fwidth(v);

    bool isOrtho = ubo.cameraPositionAndProjType.w > 0.5;
    if (isOrtho)
    {
        float minDerivative = ubo.planeAxisU.w;
        du = max(du, minDerivative);
        dv = max(dv, minDerivative);
    }
    else
    {
        du = max(du, 1e-5);
        dv = max(dv, 1e-5);
    }

    float3 gridColorMinor = float3(0.35, 0.38, 0.42);
    float3 gridColorMajor = float3(0.50, 0.55, 0.62);

    // GridPass layer weights: fracLog=0 → level0:level1 ≈ 1:1;
    // fracLog=1 → level1 transitions to level2.
    float levelWeight0 = 1.0 - fracLog;
    float levelWeight1 = 1.0;
    float levelWeight2 = fracLog;

    float totalAlpha = 0.0;
    float3 resultColor = gridColorMinor;

    // Unrolled 3-level LOD (avoids local-array indexing issues on some targets).
    {
        float stepU = max(ubo.steps[0].x, 1e-8);
        float stepV = max(ubo.steps[0].y, 1e-8);
        float dU = abs(v - round(v / stepV) * stepV);
        float dV = abs(u - round(u / stepU) * stepU);
        float emphasis = 0.0;
        float3 levelColor = lerp(gridColorMinor, gridColorMajor, emphasis);
        float halfWidth = lerp(0.5, 1.0, emphasis) * 0.5;
        // Fade only when denser than ~2 px (was 6 — too aggressive with angled fwidth).
        float densityFadeU = saturate((stepV / dv - 2.0) / 4.0);
        float densityFadeV = saturate((stepU / du - 2.0) / 4.0);
        float lineU = (1.0 - smoothstep(dv * halfWidth, dv * (halfWidth + 1.0), dU)) * densityFadeU;
        float lineV = (1.0 - smoothstep(du * halfWidth, du * (halfWidth + 1.0), dV)) * densityFadeV;
        float contrib = max(lineU, lineV) * levelWeight0;
        if (contrib >= totalAlpha)
        {
            totalAlpha = contrib;
            resultColor = levelColor;
        }
    }
    {
        float stepU = max(ubo.steps[1].x, 1e-8);
        float stepV = max(ubo.steps[1].y, 1e-8);
        float dU = abs(v - round(v / stepV) * stepV);
        float dV = abs(u - round(u / stepU) * stepU);
        float emphasis = 0.5;
        float3 levelColor = lerp(gridColorMinor, gridColorMajor, emphasis);
        float halfWidth = lerp(0.5, 1.0, emphasis) * 0.5;
        float densityFadeU = saturate((stepV / dv - 2.0) / 4.0);
        float densityFadeV = saturate((stepU / du - 2.0) / 4.0);
        float lineU = (1.0 - smoothstep(dv * halfWidth, dv * (halfWidth + 1.0), dU)) * densityFadeU;
        float lineV = (1.0 - smoothstep(du * halfWidth, du * (halfWidth + 1.0), dV)) * densityFadeV;
        float contrib = max(lineU, lineV) * levelWeight1;
        if (contrib >= totalAlpha)
        {
            totalAlpha = contrib;
            resultColor = levelColor;
        }
    }
    {
        float stepU = max(ubo.steps[2].x, 1e-8);
        float stepV = max(ubo.steps[2].y, 1e-8);
        float dU = abs(v - round(v / stepV) * stepV);
        float dV = abs(u - round(u / stepU) * stepU);
        float emphasis = 1.0;
        float3 levelColor = lerp(gridColorMinor, gridColorMajor, emphasis);
        float halfWidth = lerp(0.5, 1.0, emphasis) * 0.5;
        float densityFadeU = saturate((stepV / dv - 2.0) / 4.0);
        float densityFadeV = saturate((stepU / du - 2.0) / 4.0);
        float lineU = (1.0 - smoothstep(dv * halfWidth, dv * (halfWidth + 1.0), dU)) * densityFadeU;
        float lineV = (1.0 - smoothstep(du * halfWidth, du * (halfWidth + 1.0), dV)) * densityFadeV;
        float contrib = max(lineU, lineV) * levelWeight2;
        if (contrib >= totalAlpha)
        {
            totalAlpha = contrib;
            resultColor = levelColor;
        }
    }

    // Axis lines on top of grid.
    const float kAxisLineWidth = 3.0;
    float axisAlongU = 1.0 - smoothstep(dv * 0.5, dv * kAxisLineWidth, abs(v));
    float axisAlongV = 1.0 - smoothstep(du * 0.5, du * kAxisLineWidth, abs(u));
    float axisMask = max(axisAlongU, axisAlongV);
    if (axisMask > 0.01)
    {
        float wU = axisAlongU * ubo.axisColorU.w;
        float wV = axisAlongV * ubo.axisColorV.w;
        float3 axisColor = ubo.axisColorU.xyz;
        float axisA = wU;
        if (wV > wU)
        {
            axisColor = ubo.axisColorV.xyz;
            axisA = wV;
        }
        else if (wV > 0.01)
        {
            float wSum = wU + wV;
            axisColor = (ubo.axisColorU.xyz * wU + ubo.axisColorV.xyz * wV) / wSum;
            axisA = max(wU, wV);
        }

        float t = saturate(axisMask);
        resultColor = lerp(resultColor, axisColor, t);
        totalAlpha = max(totalAlpha, axisA);
    }

    // Horizon fade (GridPass): sin(elevation) = |ray · planeNormal|.
    if (!isOrtho)
    {
        float3 cameraPos = ubo.cameraPositionAndProjType.xyz;
        float3 rayDir = normalize(input.worldPos - cameraPos);
        float3 planeNormal = normalize(cross(ubo.planeAxisU.xyz, ubo.planeAxisV.xyz));
        float sinAngle = abs(dot(rayDir, planeNormal));
        float horizonFade = smoothstep(0.0, 0.05, sinAngle);
        totalAlpha *= horizonFade;
    }

    totalAlpha *= alphaScale;
    totalAlpha = saturate(totalAlpha);

    if (totalAlpha < 0.003)
        discard;

    return float4(resultColor, totalAlpha);
}
