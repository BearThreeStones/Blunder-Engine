// ---------- Constants ----------
static const uint GRID_STEPS_LEN  = 8;   // Total available levels
static const uint GRID_STEPS_DRAW = 3;   // Levels drawn simultaneously

struct GridUniformData
{
    float4x4 view;
    float4x4 projection;
    float4 cameraPositionAndProjType;  // xyz=pos, w=isOrtho
    float4 cameraForwardAndFarClip;    // xyz=forward, w=farClip
    float4 planeOrigin;
    float4 planeAxisU;
    float4 planeAxisV;
    float4 steps[GRID_STEPS_LEN];     // Per-level step sizes: [i].xy = stepU, stepV
    // params: x=level(float), y=numLinesPerLevel, z=alphaScale, w=pixelFac
    float4 params;
    float4 axisColorX;                 // rgb + axis line alpha scale
    float4 axisColorY;
    float4 axisColorZ;
};

[[vk::binding(0, 0)]]
ConstantBuffer<GridUniformData> ubo;

struct VertexOutput
{
    float4 sv_position : SV_Position;
    [[vk::location(0)]]
    float3 worldPos;
    [[vk::location(1)]]
    float2 coord;       // normalised [-1..1] position within grid extent
    [[vk::location(2)]]
    float alphaWeight;  // per-vertex alpha (smooth level cross-fade)
    [[vk::location(3)]]
    uint axisHighlight;  // 0 = grid, 1 = +X, 2 = +Y, 3 = +Z
    [[vk::location(4)]]
    float emphasis;      // 0 = minor grid color, 1 = major grid color
    [[vk::location(5)]]
    float2 screenLineDir;  // NDC line direction for overlay AA (encoded in lineData)
};

// ---------- Helpers ----------

float pow2f(float x) { return x * x; }
float pow3f(float x) { return x * x * x; }

bool isApproxEqual(float a, float b, float eps)
{
    return abs(a - b) < eps;
}

/// Decoded per-vertex line data.
struct LineData
{
    int   level;   // 0..GRID_STEPS_DRAW-1 (0 = finest visible, 2 = coarsest)
    uint  axis;    // 0 = line perpendicular to U, 1 = perpendicular to V
    float P;       // signed integer grid-space coordinate along the cross axis
};

/// Decode vertex ID into line level, axis, line index, and endpoint.
/// Layout: vertices are grouped by level (coarsest first), then by axis, then by line.
///   vertexId / numLines = level group (0 = coarsest = level GRID_STEPS_DRAW-1)
///   Within each level: first half = axis 0, second half = axis 1
///   Within each axis: pairs of vertices form line segments
LineData decodeLine(uint vertexId, uint numLines)
{
    LineData line;
    // Level is encoded in the upper bits: drawn from large (first) to small (last).
    line.level = int(GRID_STEPS_DRAW - 1) - int(vertexId / numLines);
    uint localId = vertexId % numLines;

    // Split into axis (first half = axis 0, second half = axis 1)
    uint halfLines = numLines / 2u;
    line.axis = localId / halfLines;
    uint axisLocalId = localId % halfLines;

    // Each line = 2 vertices, line index within this axis
    uint lineIndex = axisLocalId / 2u;
    uint linesPerAxis = halfLines / 2u;

    // Signed grid-space coordinate: centered around 0
    line.P = float(lineIndex) - float(linesPerAxis) * 0.5;
    return line;
}

/// Test if the current line is occluded by a higher (coarser) grid level.
/// A line at position P*step[level] is occluded if P*step[level] is an exact
/// multiple of step[level+1], meaning a coarser level already draws there.
bool isOccludedByHigherLevel(LineData line, int actualLevel)
{
    if (line.level < int(GRID_STEPS_DRAW) - 1 && actualLevel < int(GRID_STEPS_LEN) - 1)
    {
        float stepCurrent = ubo.steps[actualLevel][line.axis];
        float stepNext = ubo.steps[actualLevel + 1][line.axis];
        float j = abs(line.P * stepCurrent) / stepNext;
        if (isApproxEqual(round(j), j, 1e-4))
        {
            return true;
        }
    }
    return false;
}

float2 computeScreenLineDir(float3 worldStart, float3 worldEnd)
{
    float4 clip0 = mul(ubo.projection, mul(ubo.view, float4(worldStart, 1.0)));
    float4 clip1 = mul(ubo.projection, mul(ubo.view, float4(worldEnd, 1.0)));
    float2 ndc0 = clip0.xy / max(abs(clip0.w), 1e-5);
    float2 ndc1 = clip1.xy / max(abs(clip1.w), 1e-5);
    float2 d = ndc1 - ndc0;
    float len = length(d);
    if (len < 1e-5)
    {
        return float2(1.0, 0.0);
    }
    return d / len;
}


[shader("vertex")]
VertexOutput vertexMain(uint vertexId: SV_VertexID)
{
    VertexOutput output;
    output.emphasis = 0.0;

    float level      = ubo.params.x;          // continuous float level from CPU
    uint  numLines   = max(1u, (uint)ubo.params.y);  // vertices per level
    float alphaScale = ubo.params.z;
    float pixelFac   = ubo.params.w;
    bool  isOrtho    = ubo.cameraPositionAndProjType.w > 0.5;

    uint totalGridVertices = GRID_STEPS_DRAW * numLines;
    uint axisVertexCount   = 6u;

    // ---- Axis lines (always drawn, separate from grid LOD) ----
    if (vertexId >= totalGridVertices)
    {
        uint axisVertId = vertexId - totalGridVertices;
        uint axisIndex  = axisVertId / 2u;
        uint endpointId = axisVertId & 1u;

        // Axis half-extent: use the step of the coarsest drawn level × line count.
        int coarsestLevel = clamp(int(level) + int(GRID_STEPS_DRAW) - 2,
                                  0, int(GRID_STEPS_LEN) - 1);
        float axisHalfExtent = ubo.steps[coarsestLevel][0] *
                               float(numLines / 4u) * 0.5;
        float t = endpointId == 0u ? 0.0 : axisHalfExtent;
        float3 world = float3(0.0, 0.0, 0.0);
        if (axisIndex == 0u) { world.x = t; }
        else if (axisIndex == 1u) { world.y = t; }
        else { world.z = t; }

        float4 viewPos = mul(ubo.view, float4(world, 1.0));
        output.sv_position = mul(ubo.projection, viewPos);
        output.worldPos = world;
        output.coord = float2(0.0, 0.0);
        output.alphaWeight = 1.0;
        output.axisHighlight = axisIndex + 1u;
        output.emphasis = 1.0;

        float3 axisDir = float3(0.0, 0.0, 0.0);
        if (axisIndex == 0u) { axisDir = float3(1.0, 0.0, 0.0); }
        else if (axisIndex == 1u) { axisDir = float3(0.0, 1.0, 0.0); }
        else { axisDir = float3(0.0, 0.0, 1.0); }
        output.screenLineDir =
            computeScreenLineDir(float3(0.0, 0.0, 0.0), axisDir * axisHalfExtent);
        return output;
    }

    // ---- Grid lines with LOD ----
    LineData line = decodeLine(vertexId, numLines);

    // Compute the actual step-array level for this vertex.
    // Offset by -1 to force drawing one sub-level finer than the "current" level.
    int actualLevel = clamp(int(level) + line.level - 1,
                            0, int(GRID_STEPS_LEN) - 1);

    // Check occlusion by the next coarser level.
    if (isOccludedByHigherLevel(line, actualLevel))
    {
        output.sv_position = float4(0.0 / 0.0, 0.0 / 0.0, 0.0 / 0.0, 1.0);
        output.worldPos = float3(0.0);
        output.coord = float2(0.0);
        output.alphaWeight = 0.0;
        output.axisHighlight = 0u;
        output.emphasis = 0.0;
        output.screenLineDir = float2(0.0, 0.0);
        return output;
    }

    // Read per-level step sizes.
    float stepU = ubo.steps[actualLevel][0];
    float stepV = ubo.steps[actualLevel][1];
    float stepSize = line.axis == 0 ? stepV : stepU;

    // Snap grid origin to the current step size.
    float3 axisU = ubo.planeAxisU.xyz;
    float3 axisV = ubo.planeAxisV.xyz;
    float2 stepOffset = float2(
        round(dot(ubo.planeOrigin.xyz, axisU) / stepSize) * stepSize,
        round(dot(ubo.planeOrigin.xyz, axisV) / stepSize) * stepSize
    );

    // Half-extent for this level (proportional to step size and line count).
    uint linesPerAxis = numLines / 4u;
    float halfExtent = stepSize * float(linesPerAxis) * 0.5;

    // Compute endpoint along the line's running axis.
    uint localId = vertexId % numLines;
    uint endpointId = localId & 1u;
    float endpoint = endpointId == 0u ? -halfExtent : halfExtent;

    // Compute line world position.
    float lineOffset = line.P * stepSize;
    float3 world;
    if (line.axis == 0)
    {
        // Line runs along U, offset along V.
        world = axisU * (stepOffset.x + endpoint) +
                axisV * (stepOffset.y + lineOffset);
    }
    else
    {
        // Line runs along V, offset along U.
        world = axisU * (stepOffset.x + lineOffset) +
                axisV * (stepOffset.y + endpoint);
    }

    // ---- Smooth alpha cross-fade (Blender-style) ----
    float fracLevel = frac(level);

    // Emphasis: interpolate between grid color and grid_emphasis color.
    // line.level 0 → emphasis < 0 → 0 (minor color)
    // line.level 1 → emphasis transitions 1→0 as fracLevel grows (color swap)
    // line.level 2 → emphasis = 1 (always major color)
    output.emphasis = saturate(float(line.level) - fracLevel);

    // Alpha: lowest visible level fades out as fracLevel → 1.
    // line.level 0: alpha = 1-fracLevel (fades out)
    // line.level 1: alpha = 2-fracLevel (always ≥1 → saturated to 1)
    // line.level 2: alpha = 3-fracLevel (always 1)
    float levelAlpha = saturate(float(line.level) + 1.0 - fracLevel);

    // Orthographic pixel-size fade: prevent moiré when step approaches pixel size.
    if (isOrtho)
    {
        levelAlpha *= smoothstep(stepSize * 0.25, stepSize * pow3f(0.25), pixelFac);
    }

    // Normalised coordinate for edge fade.
    float normCoord = lineOffset / max(halfExtent, 0.001);
    float normEndpoint = endpoint / max(halfExtent, 0.001);
    output.coord = float2(normCoord, normEndpoint);

    float4 viewPos = mul(ubo.view, float4(world, 1.0));
    output.sv_position = mul(ubo.projection, viewPos);
    output.worldPos = world;
    output.alphaWeight = levelAlpha;
    output.axisHighlight = 0u;
    output.screenLineDir = abs(output.coord.y) > abs(output.coord.x)
        ? float2(0.0, 1.0)
        : float2(1.0, 0.0);
    return output;
}


[shader("fragment")]
void fragmentLineMrt(
    VertexOutput input,
    out float4 overlayColor : SV_Target0,
    out float4 lineData : SV_Target1)
{
    float3 cameraPos = ubo.cameraPositionAndProjType.xyz;
    bool isOrtho = ubo.cameraPositionAndProjType.w > 0.5;
    float3 cameraForward = normalize(ubo.cameraForwardAndFarClip.xyz);
    float farClip = max(ubo.cameraForwardAndFarClip.w, 0.001);
    float alphaScale = ubo.params.z;

    float3 toPixel = input.worldPos - cameraPos;
    float dist = length(toPixel);

    // ---- Edge fade ----
    float alpha;
    if (!isOrtho)
    {
        // Perspective: radial fade from center of grid extent.
        float lengthFade = 1.0 - min(1.0, length(input.coord));
        alpha = lengthFade;

        // Angle fade: lines on the floor plane fade at steep viewing angles.
        float3 V = normalize(toPixel);
        float3 planeNormal = normalize(cross(ubo.planeAxisU.xyz, ubo.planeAxisV.xyz));
        float viewDot = abs(dot(V, planeNormal));
        alpha *= 1.0 - pow3f(1.0 - viewDot);

        // Distance fade: fade towards far clip.
        alpha *= 1.0 - smoothstep(0.0, 0.5 * farClip, dist - 0.5 * farClip);
    }
    else
    {
        // Orthographic: quadratic fade at grid boundary.
        float lengthFade = 1.0 - min(1.0, dot(input.coord, input.coord));
        alpha = pow2f(lengthFade);
    }

    // Apply per-vertex level alpha and global alpha scale.
    alpha *= input.alphaWeight;
    alpha *= alphaScale;
    alpha = saturate(alpha);

    // Color: blend between minor grid color and major/emphasis color.
    float3 gridColorMinor = float3(0.55, 0.60, 0.68);
    float3 gridColorMajor = float3(0.68, 0.73, 0.80);
    float3 color = lerp(gridColorMinor, gridColorMajor, input.emphasis);

    // Axis highlight overrides.
    if (input.axisHighlight == 1u)
    {
        color = ubo.axisColorX.xyz;
        alpha = max(alpha, ubo.axisColorX.w);
    }
    else if (input.axisHighlight == 2u)
    {
        color = ubo.axisColorY.xyz;
        alpha = max(alpha, ubo.axisColorY.w);
    }
    else if (input.axisHighlight == 3u)
    {
        color = ubo.axisColorZ.xyz;
        alpha = max(alpha, ubo.axisColorZ.w);
    }

    // ---- Low-alpha stipple discard ----
    {
        const float dashWidth = 4.0;
        const float fadeStart = 0.1;
        const float fadeRcp = 1.0 / fadeStart;
        float screenDist = length(input.sv_position.xy);
        if (alpha < fadeStart && fadeRcp * alpha < frac(screenDist / dashWidth))
        {
            discard;
        }
    }

    float4 lineColor = float4(color, alpha);

    float2 lineDir = input.screenLineDir;
    if (length(lineDir) < 1e-4)
    {
        lineDir = float2(1.0, 0.0);
        if (abs(input.coord.y) > abs(input.coord.x))
        {
            lineDir = float2(0.0, 1.0);
        }
    }

    overlayColor = lineColor;
    float signedDist = (lineColor.a - 0.5) * 2.0;
    lineData = float4(lineDir * 0.5 + 0.5, signedDist * 0.5 + 0.5, 0.0);
}
