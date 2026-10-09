// camera_gizmo.slang — batched Camera / Light overlay wires (ScreenOverlayPass).

static const float kPolylineSmoothWidth = 0.5;
static const float kLineEndPadPx = 1.0;
static const uint kLineVertCount = 6u;
static const uint kTriangleVertCount = 3u;
static const uint kOriginDiscVertCount = 6u;
static const uint kIconVertCount = 6u;
static const float kOriginDiscRadiusPx = 3.0;
/// Screen-space half-extent of Unique icon billboards (pixels).
static const float kIconHalfExtentPx = 18.0;
static const float3 kIconFill = float3(0.969, 0.961, 0.812); // #f7f5cf
static const float3 kIconOutline = float3(0.08, 0.08, 0.08);

struct CameraGizmoUniform {
    float4x4 view;
    float4x4 proj;
    float line_width_px;
    float viewport_width_px;
    float viewport_height_px;
    float command_count;
};

/// 80-byte std430 row. `float3` pad would align to 16 and stride 96, which
/// misreads CPU `GizmoLineCommand` (80) so line `p2` (origin) becomes an
/// endpoint — Collision Debug starburst. Keep in sync with debug_draw.slang.
struct GizmoLineCommand {
    float4 p0;
    float4 p1;
    float4 p2;
    float4 color;
    float style;
    float width_px;
    float2 _pad;
};

[[vk::binding(0, 0)]]
ConstantBuffer<CameraGizmoUniform> ubo;

[[vk::binding(1, 0)]]
StructuredBuffer<GizmoLineCommand> commands;

struct VertexOutput {
    float4 sv_position : SV_Position;
    [[vk::location(0)]]
    float4 color;
    [[vk::location(1)]]
    nointerpolation float style;
    [[vk::location(2)]]
    float2 disc_uv;
};

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

float4 worldToClip(float3 w) {
    return mul(ubo.proj, mul(ubo.view, float4(w, 1.0)));
}

VertexOutput vsLine(uint vid, GizmoLineCommand cmd) {
    float3 startWorld = cmd.p0.xyz;
    float3 endWorld = cmd.p1.xyz;
    float3 worldDir = endWorld - startWorld;
    if (length(worldDir) < 1e-6) {
        return emitDiscarded();
    }

    float2 ap = getArmParams(vid);
    float3 worldPos = lerp(startWorld, endWorld, ap.x);
    float4 Mv = worldToClip(worldPos);
    if (abs(Mv.w) < 1e-6) {
        return emitDiscarded();
    }

    float4 Mn = worldToClip(worldPos + worldDir);
    float4 delta = (Mn - Mv * (Mn.w / Mv.w)) / Mv.w;
    float2 vp = float2(max(ubo.viewport_width_px, 1.0),
                       max(ubo.viewport_height_px, 1.0));
    float2 pixelDelta = delta.xy * vp;
    float pixelLen = length(pixelDelta);
    if (pixelLen < 1e-8) {
        return emitDiscarded();
    }
    pixelDelta /= pixelLen;
    float2 screenNormal = float2(-pixelDelta.y, pixelDelta.x) / vp;
    float pixelWidth = ubo.line_width_px + kPolylineSmoothWidth;
    float4 outpos = (Mv / Mv.w) + float4(screenNormal * ap.y * pixelWidth * 0.5, 0, 0);
    float forwards = (ap.x - 0.5) * 2.0;
    outpos.xy += forwards * (pixelDelta / vp) * 0.5 * kLineEndPadPx;
    outpos *= Mv.w;

    VertexOutput o;
    o.sv_position = outpos;
    o.color = cmd.color;
    o.style = 0.0;
    o.disc_uv = float2(ap.y * 0.5 + 0.5, ap.x);
    return o;
}

VertexOutput vsTriangle(uint vid, GizmoLineCommand cmd) {
    float3 p;
    if (vid == 0u) {
        p = cmd.p0.xyz;
    } else if (vid == 1u) {
        p = cmd.p1.xyz;
    } else {
        p = cmd.p2.xyz;
    }

    VertexOutput o;
    o.sv_position = worldToClip(p);
    o.color = cmd.color;
    o.style = 1.0;
    o.disc_uv = float2(0, 0);
    return o;
}

VertexOutput vsOriginDisc(uint vid, GizmoLineCommand cmd) {
    float4 clipC = worldToClip(cmd.p0.xyz);
    float radiusClip = kOriginDiscRadiusPx * 2.0 / max(ubo.viewport_height_px, 1.0) * abs(clipC.w);
    float2 uv = quadUV(vid);
    float2 offset = uv * radiusClip;

    VertexOutput o;
    o.sv_position = float4(clipC.xy + offset, clipC.z, clipC.w);
    o.color = cmd.color;
    o.style = 2.0;
    o.disc_uv = uv;
    return o;
}

/// Screen-aligned billboard at world p0 (always faces viewport, upright on screen).
VertexOutput vsIconBillboard(uint vid, GizmoLineCommand cmd) {
    float4 clipC = worldToClip(cmd.p0.xyz);
    if (clipC.w <= 1e-5) {
        return emitDiscarded();
    }
    float halfClip =
        kIconHalfExtentPx * 2.0 / max(ubo.viewport_height_px, 1.0) * abs(clipC.w);
    float2 uv = quadUV(vid);
    float2 offset = uv * halfClip;

    VertexOutput o;
    o.sv_position = float4(clipC.xy + offset, clipC.z, clipC.w);
    o.color = cmd.color;
    o.style = cmd.style;
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

/// Godot GizmoLight-like bulb in UV [-1,1].
float lightBulbSdf(float2 uv) {
    float2 p = uv * 1.15;
    float globe = sdCircle(p - float2(0.0, 0.20), 0.46);
    float neck = sdRoundedBox(p - float2(0.0, -0.28), float2(0.16, 0.12), 0.04);
    float base = sdRoundedBox(p - float2(0.0, -0.52), float2(0.26, 0.12), 0.04);
    float d = min(globe, neck);
    d = min(d, base);
    return d;
}

[shader("vertex")]
VertexOutput vertexMain(uint vid : SV_VertexID, uint iid : SV_InstanceID) {
    uint count = uint(ubo.command_count + 0.5);
    if (iid >= count) {
        return emitDiscarded();
    }
    GizmoLineCommand cmd = commands[iid];
    uint style = uint(cmd.style + 0.5);
    if (style == 0u) {
        if (vid >= kLineVertCount) {
            return emitDiscarded();
        }
        return vsLine(vid, cmd);
    }
    if (style == 1u) {
        if (vid >= kTriangleVertCount) {
            return emitDiscarded();
        }
        return vsTriangle(vid, cmd);
    }
    if (style == 2u) {
        if (vid >= kOriginDiscVertCount) {
            return emitDiscarded();
        }
        return vsOriginDisc(vid, cmd);
    }
    if (style == 3u || style == 4u) {
        if (vid >= kIconVertCount) {
            return emitDiscarded();
        }
        return vsIconBillboard(vid, cmd);
    }
    return emitDiscarded();
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    if (input.color.a < 0.001) {
        discard;
    }

    float4 color = input.color;
    if (input.style < 0.5 && input.style > -0.5) {
        float dist = abs((input.disc_uv.x - 0.5) * 2.0);
        float aa = max(fwidth(dist), 1e-4);
        color.a *= saturate((1.0 - dist) / aa);
        if (color.a < 0.001) {
            discard;
        }
        return color;
    }
    if (input.style > 3.5) {
        float d = lightBulbSdf(input.disc_uv);
        float aa = max(fwidth(d), 1e-4);
        float fill = 1.0 - smoothstep(-aa, aa, d);
        float stroke = 1.0 - smoothstep(-aa, aa, abs(d) - 0.07);

        float3 fillRgb = lerp(kIconFill, input.color.rgb, 0.35 * saturate(input.color.a));
        float3 outlineRgb = lerp(kIconOutline, input.color.rgb, 0.55);
        float3 rgb = lerp(outlineRgb, fillRgb, fill);
        float a = max(fill, stroke) * saturate(input.color.a);
        if (a < 0.001) {
            discard;
        }
        return float4(rgb, a);
    }
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
