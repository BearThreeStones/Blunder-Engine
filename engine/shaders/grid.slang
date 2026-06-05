// grid.slang — Analytical anti-aliased ground-plane grid with LOD.
//
// Renders as a large quad on the grid plane using TriangleList topology
// (6 vertices from SV_VertexID).  Grid lines are computed analytically
// per pixel in the fragment shader using fwidth() for perfect AA at any
// zoom level.

static const uint GRID_STEPS_LEN  = 8u;
static const uint GRID_STEPS_DRAW = 3u;

struct GridUniformData
{
    float4x4 view;
    float4x4 projection;
    float4 cameraPositionAndProjType;  // xyz=pos, w=isOrtho
    float4 cameraForwardAndFarClip;    // xyz=forward, w=farClip
    float4 quadCenter;                 // xyz = camera projection onto grid plane
    float4 planeAxisU;                 // xyz = first grid axis
    float4 planeAxisV;                 // xyz = second grid axis
    float4 steps[GRID_STEPS_LEN];     // [i].xy = step size for level i
    // params: x=level (CPU hint), y=quadHalfExtent, z=alphaScale, w=cameraDistance
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

// ---------- Vertex Shader ----------

[shader("vertex")]
VertexOutput vertexMain(uint vertexId : SV_VertexID)
{
    VertexOutput output;

    float extent = ubo.params.y;

    // Two triangles forming a quad: (BL, BR, TR), (BL, TR, TL).
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
    // Exact world-space UV (not relative to quad center, but to world origin).
    output.planeUV = float2(dot(world, axisU), dot(world, axisV));
    return output;
}

// ---------- Helpers ----------

float pow3f(float x) { return x * x * x; }

// ---------- Fragment Shader ----------

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    float u = input.planeUV.x;
    float v = input.planeUV.y;

    float alphaScale = ubo.params.z;

    // Screen-space derivatives (world units per pixel) — scale with perspective.
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

    // Use CPU-computed level as the base level.
    float level = ubo.params.x;
    float fracLevel = frac(level);

    float3 gridColorMinor = float3(0.35, 0.38, 0.42);
    float3 gridColorMajor = float3(0.50, 0.55, 0.62);

    float totalAlpha = 0.0;
    float3 resultColor = gridColorMinor;

    // --- Multi-level LOD grid lines ---
    for (int i = 0; i < int(GRID_STEPS_DRAW); i++)
    {
        int actualLevel = clamp(int(level) + i - 1, 0, int(GRID_STEPS_LEN) - 1);
        float stepU = ubo.steps[actualLevel].x;
        float stepV = ubo.steps[actualLevel].y;

        // Compute distance to nearest grid line center in world units using safe round() to support negative quadrants.
        float dU = abs(v - round(v / stepV) * stepV);
        float dV = abs(u - round(u / stepU) * stepU);

        // Zoom-based alpha transition.
        float levelAlpha = saturate(float(i) + 1.0 - fracLevel);

        // Color emphasis (fine → minor, coarse → major).
        float emphasis = saturate(float(i) - fracLevel);
        float3 levelColor = lerp(gridColorMinor, gridColorMajor, emphasis);

        // Dynamic line width: minor lines are thinner (0.5 pixels), major lines are standard (1.0 pixel).
        float lineWidth = lerp(0.5, 1.0, emphasis);
        float halfWidth = lineWidth * 0.5;

        // Screen-space density-based fade to prevent Moire and aliasing when lines are too close.
        float densityFadeU = saturate((stepV / dv - 6.0) / 4.0);
        float densityFadeV = saturate((stepU / du - 6.0) / 4.0);

        // Anti-aliased coverage: dynamically sized line, independently faded by screen-space density.
        float lineU = (1.0 - smoothstep(dv * halfWidth, dv * (halfWidth + 1.0), dU)) * densityFadeU;
        float lineV = (1.0 - smoothstep(du * halfWidth, du * (halfWidth + 1.0), dV)) * densityFadeV;
        float line = max(lineU, lineV);

        float contrib = line * levelAlpha;

        // Coarser levels override finer levels (greater-or-equal test).
        if (contrib >= totalAlpha)
        {
            totalAlpha = contrib;
            resultColor = levelColor;
        }
    }

    // --- Axis lines: composite on top of grid (grid intersections must not occlude axes) ---
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

    // Horizon fade: fade out the grid as we get far from the camera focal center
    // to match the quad extent boundary nicely and prevent abrupt edges
    if (!isOrtho)
    {
        float3 focal = ubo.quadCenter.xyz;
        float distToFocal = length(input.worldPos - focal);
        float horizonFade = 1.0 - smoothstep(ubo.params.y * 0.5, ubo.params.y * 0.9, distToFocal);
        totalAlpha *= horizonFade;
    }

    totalAlpha *= alphaScale;
    totalAlpha = saturate(totalAlpha);

    if (totalAlpha < 0.003)
        discard;

    return float4(resultColor, totalAlpha);
}
