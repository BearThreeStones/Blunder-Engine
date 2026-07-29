// camera_gizmo.slang — Blender-like Camera Gizmo wire overlay (ScreenOverlayPass).

static const float kPolylineSmoothWidth = 0.5;
static const uint kLineVertCount = 6u;
static const uint kTriangleVertCount = 3u;
static const uint kOriginDiscVertCount = 6u;
static const uint kIconVertCount = 6u;
static const float kOriginDiscRadiusPx = 3.0;
/// Screen-space half-extent of the camera billboard icon (pixels).
static const float kIconHalfExtentPx = 18.0;
static const float3 kIconFill = float3(0.969, 0.961, 0.812); // #f7f5cf
static const float3 kIconOutline = float3(0.08, 0.08, 0.08);

struct CameraGizmoUniform {
    float4x4 view;
    float4x4 proj;
    float4 color;
    float line_width_px;
    float viewport_height_px;
    float style;
    float _pad;
    float4 p0;
    float4 p1;
    float4 p2;
};

[[vk::binding(0, 0)]]
ConstantBuffer<CameraGizmoUniform> ubo;

struct VertexOutput {
    float4 sv_position : SV_Position;
    [[vk::location(0)]]
    float4 color;
    [[vk::location(1)]]
    nointerpolation float style;
    [[vk::location(2)]]
    float2 disc_uv;
};

float3 worldToView(float3 w) {
    return mul(ubo.view, float4(w, 1.0)).xyz;
}

float2 getArmParams(uint idx) {
    switch (idx) {
        case 0: return float2(0, -1);
        case 1: return float2(1, -1);
        case 2: return float2(1, 1);
        case 3: return float2(0, -1);
        case 4: return float2(1, 1);
        default: return float2(0, 1);
    }
}

float2 quadUV(uint idx) {
    switch (idx) {
        case 0: return float2(-1, -1);
        case 1: return float2(1, -1);
        case 2: return float2(1, 1);
        case 3: return float2(-1, -1);
        case 4: return float2(1, 1);
        default: return float2(-1, 1);
    }
}

VertexOutput emitDiscarded() {
    VertexOutput o;
    o.sv_position = float4(0, 0, 2, 1);
    o.color = float4(0, 0, 0, 0);
    o.style = -1.0;
    o.disc_uv = float2(0, 0);
    return o;
}

VertexOutput vsLine(uint vid) {
    float3 startWorld = ubo.p0.xyz;
    float3 endWorld = ubo.p1.xyz;
    float3 startView = worldToView(startWorld);
    float3 endView = worldToView(endWorld);
    float3 armDelta = endView - startView;
    float armLen = length(armDelta);
    if (armLen < 1e-6) {
        return emitDiscarded();
    }

    float3 armDir = armDelta / armLen;
    float3 perp = cross(armDir, float3(0.0, 0.0, 1.0));
    float perpLen = length(perp);
    float3 armPerp = (perpLen > 1e-3) ? (perp / perpLen) : float3(1.0, 0.0, 0.0);

    float4 clipStart = mul(ubo.proj, float4(startView, 1.0));
    float4 clipEnd = mul(ubo.proj, float4(endView, 1.0));
    float startScale = abs(clipStart.w);
    float endScale = abs(clipEnd.w);
    float armHalfW = (ubo.line_width_px + kPolylineSmoothWidth) * 0.5 *
                     (2.0 / max(ubo.viewport_height_px, 1.0));
    armPerp *= armHalfW * (startScale + endScale) * 0.5;

    float2 ap = getArmParams(vid);
    float3 cornerView = lerp(startView, endView, ap.x) + armPerp * ap.y;

    VertexOutput o;
    o.sv_position = mul(ubo.proj, float4(cornerView, 1.0));
    o.color = ubo.color;
    o.style = 0.0;
    o.disc_uv = float2(0, 0);
    return o;
}

VertexOutput vsTriangle(uint vid) {
    float3 p;
    if (vid == 0u) {
        p = ubo.p0.xyz;
    } else if (vid == 1u) {
        p = ubo.p1.xyz;
    } else {
        p = ubo.p2.xyz;
    }

    VertexOutput o;
    o.sv_position = mul(ubo.proj, mul(ubo.view, float4(p, 1.0)));
    o.color = ubo.color;
    o.style = 1.0;
    o.disc_uv = float2(0, 0);
    return o;
}

VertexOutput vsOriginDisc(uint vid) {
    float4 clipC = mul(ubo.proj, mul(ubo.view, float4(ubo.p0.xyz, 1.0)));
    float radiusClip = kOriginDiscRadiusPx * 2.0 / max(ubo.viewport_height_px, 1.0) * abs(clipC.w);
    float2 uv = quadUV(vid);
    float2 offset = uv * radiusClip;

    VertexOutput o;
    o.sv_position = float4(clipC.xy + offset, clipC.z, clipC.w);
    o.color = ubo.color;
    o.style = 2.0;
    o.disc_uv = uv;
    return o;
}

/// Screen-aligned billboard at world p0 (always faces viewport, upright on screen).
VertexOutput vsIconBillboard(uint vid) {
    float4 clipC = mul(ubo.proj, mul(ubo.view, float4(ubo.p0.xyz, 1.0)));
    if (clipC.w <= 1e-5) {
        return emitDiscarded();
    }
    float halfClip =
        kIconHalfExtentPx * 2.0 / max(ubo.viewport_height_px, 1.0) * abs(clipC.w);
    float2 uv = quadUV(vid);
    float2 offset = uv * halfClip;

    VertexOutput o;
    o.sv_position = float4(clipC.xy + offset, clipC.z, clipC.w);
    o.color = ubo.color;
    o.style = 3.0;
    o.disc_uv = uv;
    return o;
}

float sdCircle(float2 p, float r) {
    return length(p) - r;
}

float sdRoundedBox(float2 p, float2 b, float r) {
    float2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

float sdTriangleIsosceles(float2 p, float2 q) {
    p.x = abs(p.x);
    float2 a = p - q * saturate(dot(p, q) / dot(q, q));
    float2 b = p - float2(saturate(p.x / q.x) * q.x, q.y);
    float s = -sign(q.y);
    float2 d = min(float2(dot(a, a), s * (p.x * q.y - p.y * q.x)),
                   float2(dot(b, b), s * (p.y - q.y)));
    return -sqrt(d.x) * sign(d.y);
}

/// Godot GizmoCamera3D-like silhouette in UV [-1,1] (lens points +X / right).
float cameraIconSdf(float2 uv) {
    // Slight padding so stroke stays inside the billboard quad.
    float2 p = uv * 1.15;

    float reelL = sdCircle(p - float2(-0.32, -0.42), 0.30);
    float reelR = sdCircle(p - float2(0.18, -0.52), 0.30);
    float body = sdRoundedBox(p - float2(0.0, 0.22), float2(0.48, 0.38), 0.10);
    // Lens: isosceles triangle pointing +X, anchored on body right edge.
    float2 lp = float2(-(p.y - 0.12), p.x - 0.35);
    float lens = sdTriangleIsosceles(lp, float2(0.42, 0.55));

    float d = min(reelL, reelR);
    d = min(d, body);
    d = min(d, lens);
    return d;
}

[shader("vertex")]
VertexOutput vertexMain(uint vid : SV_VertexID) {
    uint style = uint(ubo.style + 0.5);
    if (style == 0u) {
        if (vid >= kLineVertCount) {
            return emitDiscarded();
        }
        return vsLine(vid);
    }
    if (style == 1u) {
        if (vid >= kTriangleVertCount) {
            return emitDiscarded();
        }
        return vsTriangle(vid);
    }
    if (style == 2u) {
        if (vid >= kOriginDiscVertCount) {
            return emitDiscarded();
        }
        return vsOriginDisc(vid);
    }
    if (style == 3u) {
        if (vid >= kIconVertCount) {
            return emitDiscarded();
        }
        return vsIconBillboard(vid);
    }
    return emitDiscarded();
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    if (input.color.a < 0.001) {
        discard;
    }

    float4 color = input.color;
    if (input.style > 2.5) {
        float d = cameraIconSdf(input.disc_uv);
        float aa = max(fwidth(d), 1e-4);
        float fill = 1.0 - smoothstep(-aa, aa, d);
        float stroke = smoothstep(-aa, aa, abs(d) - 0.07);
        stroke = 1.0 - stroke;

        float3 fillRgb = lerp(kIconFill, input.color.rgb, 0.35 * saturate(input.color.a));
        float3 outlineRgb = lerp(kIconOutline, input.color.rgb, 0.55);
        float3 rgb = lerp(outlineRgb, fillRgb, fill);
        float a = max(fill, stroke) * saturate(input.color.a);
        if (a < 0.001) {
            discard;
        }
        return float4(rgb, a);
    }
    if (input.style > 1.5) {
        float d = length(input.disc_uv);
        float aa = fwidth(d);
        color.a *= saturate((1.0 - d) / max(aa, 1e-4));
        if (color.a < 0.001) {
            discard;
        }
    }
    return color;
}
