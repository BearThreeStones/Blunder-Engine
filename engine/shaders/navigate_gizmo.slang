// navigate_gizmo.slang — Blender-style 3D orientation gizmo
// Renders via SV_VertexID (no vertex buffers), TriangleList topology.
//
// Vertex layout (84 total):
//   [0-5]    Background disc
//   [6-11]   Center dot
//   [12-83]  6 sorted axis slots × 12 verts each (arm quad + ball quad)

static const uint VERT_BG_END      = 6u;
static const uint VERT_CENTER_END  = 12u;
static const uint VERTS_PER_SLOT   = 12u;

struct GizmoUniformData
{
    float4x4 viewRotation;
    float4x4 projection;
    float4   axisColorX;       // rgb + alpha
    float4   axisColorY;
    float4   axisColorZ;
    float4   centerColor;      // rgb + alpha
    float4   bgColor;          // rgb + alpha
    // params: x=armLength, y=ballRadius, z=centerRadius, w=negBallRadius
    float4   params;
    // params2: x=armHalfWidth, y=negAlphaScale, z=bgRadius, w=letterThreshold
    float4   params2;
    float4   sortOrder0;       // 4 sorted indices (back-to-front)
    float4   sortOrder1;       // 2 sorted indices + padding
};

[[vk::binding(0, 0)]]
ConstantBuffer<GizmoUniformData> ubo;

struct VertexOutput
{
    float4 sv_position : SV_Position;
    [[vk::location(0)]]
    float2 uv;
    [[vk::location(1)]]
    nointerpolation float4 color;
    [[vk::location(2)]]
    nointerpolation uint primType;   // 0=bg, 1=center, 2=arm, 3=posBall, 4=negBall
    [[vk::location(3)]]
    nointerpolation uint axisIndex;  // 0=X, 1=Y, 2=Z
};

// ---------- Quad helpers ----------

float2 getQuadUV(uint idx)
{
    // Two triangles: (0,1,2), (3,4,5) = (BL,BR,TR), (BL,TR,TL)
    switch (idx) {
        case 0: return float2(-1, -1);
        case 1: return float2( 1, -1);
        case 2: return float2( 1,  1);
        case 3: return float2(-1, -1);
        case 4: return float2( 1,  1);
        default: return float2(-1,  1);
    }
}

float2 getArmParams(uint idx)
{
    // x = along (0=start, 1=end), y = perp (-1 or +1)
    switch (idx) {
        case 0: return float2(0, -1);
        case 1: return float2(1, -1);
        case 2: return float2(1,  1);
        case 3: return float2(0, -1);
        case 4: return float2(1,  1);
        default: return float2(0,  1);
    }
}

int getSortedIndex(uint slot)
{
    if (slot < 4u) return int(ubo.sortOrder0[slot]);
    return int(ubo.sortOrder1[slot - 4u]);
}

float3 getWorldAxis(uint axisIdx)
{
    if (axisIdx == 0u) return float3(1, 0, 0);
    if (axisIdx == 1u) return float3(0, 1, 0);
    return float3(0, 0, 1);
}

float4 getAxisColor(uint axisIdx)
{
    if (axisIdx == 0u) return ubo.axisColorX;
    if (axisIdx == 1u) return ubo.axisColorY;
    return ubo.axisColorZ;
}

float4 projectPos3D(float3 pos3D)
{
    return mul(ubo.projection, float4(pos3D, 1.0));
}

// ---------- Letter SDF ----------

float sdSegment2D(float2 p, float2 a, float2 b)
{
    float2 pa = p - a;
    float2 ba = b - a;
    float h = saturate(dot(pa, ba) / dot(ba, ba));
    return length(pa - ba * h);
}

float letterX(float2 uv)
{
    float d1 = sdSegment2D(uv, float2(-0.5, -0.7), float2(0.5, 0.7));
    float d2 = sdSegment2D(uv, float2(-0.5, 0.7), float2(0.5, -0.7));
    return min(d1, d2);
}

float letterY(float2 uv)
{
    float d1 = sdSegment2D(uv, float2(-0.5, 0.7), float2(0.0, 0.0));
    float d2 = sdSegment2D(uv, float2(0.5, 0.7), float2(0.0, 0.0));
    float d3 = sdSegment2D(uv, float2(0.0, 0.0), float2(0.0, -0.7));
    return min(min(d1, d2), d3);
}

float letterZ(float2 uv)
{
    float d1 = sdSegment2D(uv, float2(-0.5, 0.7), float2(0.5, 0.7));
    float d2 = sdSegment2D(uv, float2(0.5, 0.7), float2(-0.5, -0.7));
    float d3 = sdSegment2D(uv, float2(-0.5, -0.7), float2(0.5, -0.7));
    return min(min(d1, d2), d3);
}

float letterDist(float2 uv, uint axisIdx)
{
    if (axisIdx == 0u) return letterX(uv);
    if (axisIdx == 1u) return letterY(uv);
    return letterZ(uv);
}

// ---------- Vertex Shader ----------

[shader("vertex")]
VertexOutput vertexMain(uint vertexId : SV_VertexID)
{
    VertexOutput output;
    output.uv = float2(0, 0);
    output.color = float4(0, 0, 0, 0);
    output.primType = 0u;
    output.axisIndex = 0u;

    float armLength   = ubo.params.x;
    float ballRadius  = ubo.params.y;
    float centerRad   = ubo.params.z;
    float negBallRad  = ubo.params.w;
    float armHalfW    = ubo.params2.x;
    float negAlpha    = ubo.params2.y;
    float bgRadius    = ubo.params2.z;

    float D = ubo.sortOrder1.z;
    float perspectiveFactor = ubo.sortOrder1.w;
    float3 viewPosCenter = float3(0.0, 0.0, -D);

    // ---- Background disc ----
    if (vertexId < VERT_BG_END)
    {
        output.primType = 0u;
        float2 uv = getQuadUV(vertexId);
        output.uv = uv;
        output.color = ubo.bgColor;
        float3 pos3D = viewPosCenter + float3(uv * bgRadius, -0.01);
        output.sv_position = projectPos3D(pos3D);
        return output;
    }

    // ---- Center dot ----
    if (vertexId < VERT_CENTER_END)
    {
        output.primType = 1u;
        float2 uv = getQuadUV(vertexId - VERT_BG_END);
        output.uv = uv;
        output.color = ubo.centerColor;
        float3 pos3D = viewPosCenter + float3(uv * centerRad, 0.0);
        output.sv_position = projectPos3D(pos3D);
        return output;
    }

    // ---- Sorted axis elements ----
    uint axisVertId = vertexId - VERT_CENTER_END;
    uint slot = axisVertId / VERTS_PER_SLOT;
    uint slotLocal = axisVertId % VERTS_PER_SLOT;

    int endpointIdx = getSortedIndex(slot);
    uint axisIdx = uint(endpointIdx) / 2u;
    bool isPositive = (uint(endpointIdx) % 2u == 0u);
    output.axisIndex = axisIdx;

    float3 worldDir = getWorldAxis(axisIdx);
    if (!isPositive) worldDir = -worldDir;

    // Rotate world axis by view rotation (translation stripped on CPU).
    float3 viewDir3D = mul(ubo.viewRotation, float4(worldDir, 0.0)).xyz;
    float3 endPos = viewPosCenter + viewDir3D * armLength;
    float4 axisColor = getAxisColor(axisIdx);

    float visualScale = lerp(1.0, D / -endPos.z, perspectiveFactor);

    if (slotLocal < 6u)
    {
        // ---- Arm quad ----
        output.primType = 2u;

        float3 startPos = viewPosCenter + viewDir3D * (centerRad * 0.5);
        float3 armDelta = endPos - startPos;
        float armScreenLen = length(armDelta);

        if (armScreenLen < 0.001)
        {
            // Degenerate arm — collapse
            output.sv_position = float4(0.0, 0.0, -2.0, 1.0);
            output.color = float4(0, 0, 0, 0);
            return output;
        }

        float3 armDir3D = armDelta / armScreenLen;
        float3 perp = cross(armDir3D, float3(0.0, 0.0, 1.0));
        float perpLen = length(perp);
        float3 armPerp = (perpLen > 0.001) ? (perp / perpLen * armHalfW) : float3(armHalfW, 0.0, 0.0);

        // Scale the perp vector by the average visual scale of start/end points:
        float startScale = lerp(1.0, D / -startPos.z, perspectiveFactor);
        float scale = (startScale + visualScale) * 0.5;
        armPerp *= scale;

        float2 ap = getArmParams(slotLocal);
        float3 basePos = lerp(startPos, endPos, ap.x);
        float3 cornerPos = basePos + armPerp * ap.y;

        output.sv_position = projectPos3D(cornerPos);
        float armAlpha = isPositive ? 0.85 : (0.85 * negAlpha);
        output.color = float4(axisColor.xyz, armAlpha);
        output.uv = float2(0, 0);
        return output;
    }
    else
    {
        // ---- Ball circle ----
        uint qv = slotLocal - 6u;
        float2 uv = getQuadUV(qv);
        output.uv = uv;

        float rad = isPositive ? ballRadius : negBallRad;
        output.primType = isPositive ? 3u : 4u;

        // Billboard offset in view-space XY plane, scaled by perspective:
        float3 ballPos = endPos + float3(uv * rad * visualScale, 0.0);
        output.sv_position = projectPos3D(ballPos);

        float alpha = isPositive ? axisColor.w : (axisColor.w * negAlpha);
        output.color = float4(axisColor.xyz, alpha);
        return output;
    }
}

// ---------- Fragment Shader ----------

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    float2 uv = input.uv;
    float4 color = input.color;
    uint primType = input.primType;

    // Circles: background disc, center dot, positive ball, negative ball
    if (primType != 2u)
    {
        float dist = length(uv);
        if (dist > 1.0) discard;

        // Smooth edge anti-aliasing
        float edgeAlpha = 1.0 - smoothstep(0.82, 1.0, dist);
        color.a *= edgeAlpha;

        // Letter rendering on positive axis balls only
        if (primType == 3u)
        {
            float lt = ubo.params2.w;
            // Scale UV so letters fill ~55% of ball
            float2 letterUV = uv * 1.8;
            float ld = letterDist(letterUV, input.axisIndex);
            float letterAlpha = 1.0 - smoothstep(lt * 0.6, lt, ld);

            // Contrast: white letters on dark axes, dark on bright
            float3 letterColor = float3(1.0, 1.0, 1.0);
            float lum = dot(color.xyz, float3(0.299, 0.587, 0.114));
            if (lum > 0.5) letterColor = float3(0.12, 0.12, 0.12);

            color.xyz = lerp(color.xyz, letterColor, letterAlpha * 0.92);
        }
    }
    // primType == 2u: arm — just use solid color as-is

    return color;
}
