// transform_gizmo.slang — translate handles + rotate dials + ghost arc
//
// Plane quads (style 1) are the proven visible path; quad_layout + quad_z
// parameterize position/size. Arrows use three cross billboards (style 0).

static const uint kArrowQuads = 3u;
static const uint kArrowVertCount = kArrowQuads * 6u;
static const uint kPlaneVertCount = 6u;
static const uint kCenterSides = 12u;
static const uint kCenterVertCount = kCenterSides * 6u;
static const uint kDialSides = 48u;
static const uint kDialVertCount = kDialSides * 6u;
static const uint kScaleBoxVertCount = 36u;

struct TransformGizmoUniform {
    float4x4 view;
    float4x4 proj;
    float4x4 gizmoWorld;
    float4 color;
    // x=style, y=alpha, z=line_width_scale or arc_start_rad (dial ghost), w=arc_delta_rad or scale uniform flag
    float4 params;
    // xy = center, zw = half extents (local XY plane)
    float4 quad_layout;
    float quad_z;
};

[[vk::binding(0, 0)]]
ConstantBuffer<TransformGizmoUniform> ubo;

struct VertexOutput {
    float4 sv_position : SV_Position;
    [[vk::location(0)]]
    float3 worldNormal;
    [[vk::location(1)]]
    float4 color;
};

float2 quadUV(uint idx) {
    switch (idx) {
        case 0: return float2(0, 0);
        case 1: return float2(1, 0);
        case 2: return float2(1, 1);
        case 3: return float2(0, 0);
        case 4: return float2(1, 1);
        default: return float2(0, 1);
    }
}

float4 projectLocal(float3 localPos) {
    float4 world = mul(ubo.gizmoWorld, float4(localPos, 1.0));
    return mul(ubo.proj, mul(ubo.view, world));
}

float3 normalFromLocal(float3 localNormal) {
    float3x3 m = (float3x3)mul(ubo.view, ubo.gizmoWorld);
    return normalize(mul(m, localNormal));
}

float lineWidthScale() {
    uint style = uint(ubo.params.x + 0.5);
    if (style == 4u) {
        return 1.5;
    }
    if (style == 0u) {
        return max(ubo.params.z, 1.0);
    }
    if (style == 3u || style == 5u) {
        return max(ubo.params.z, 1.0);
    }
    return max(ubo.params.z, 0.5);
}

VertexOutput emitV(float3 p, float3 n) {
    VertexOutput o;
    o.sv_position = projectLocal(p);
    o.worldNormal = normalFromLocal(n);
    o.color = float4(ubo.color.rgb, ubo.color.a * ubo.params.y);
    return o;
}

VertexOutput emitDiscarded() {
    VertexOutput o;
    o.sv_position = float4(0, 0, 2, 1);
    o.worldNormal = float3(0, 0, 1);
    o.color = float4(0, 0, 0, 0);
    return o;
}

// Parameterized billboard quad in local XY (same path as visible plane handles).
VertexOutput vsPlane(uint vid) {
    float cx = ubo.quad_layout.x;
    float cy = ubo.quad_layout.y;
    float hx = ubo.quad_layout.z;
    float hy = ubo.quad_layout.w;
    float2 uv = quadUV(vid);
    float3 p = float3(lerp(cx - hx, cx + hx, uv.x), lerp(cy - hy, cy + hy, uv.y), ubo.quad_z);
    return emitV(p, float3(0, 0, 1));
}

// Three stem quads rotated 120° around +Z (width in XY, length along +Z).
VertexOutput vsArrow(uint vid) {
    const float width = lineWidthScale();
    const float stemHalfW = 0.06 * width;
    const float stemHalfLen = 0.42 * width;
    const float stemCenterZ = 0.48;

    uint quad = vid / 6u;
    uint corner = vid % 6u;
    if (quad >= kArrowQuads) {
        return emitDiscarded();
    }

    float ang = float(quad) * 2.0943951; // 2*pi/3
    float ca = cos(ang);
    float sa = sin(ang);

    float2 uv = quadUV(corner);
    float lw = lerp(-stemHalfW, stemHalfW, uv.x);
    float lz = lerp(stemCenterZ - stemHalfLen, stemCenterZ + stemHalfLen, uv.y);
    float3 p = float3(lw * sa, lw * ca, lz);
    float3 n = normalize(float3(sa, ca, 0.0));
    return emitV(p, n);
}

VertexOutput vsCenter(uint vid) {
    const float r = 0.22 * lineWidthScale();
    uint side = vid / 6u;
    uint corner = vid % 6u;
    float ang0 = float(side) / float(kCenterSides) * 6.2831853;
    float ang1 = float(side + 1u) / float(kCenterSides) * 6.2831853;
    float tip = (corner == 2u || corner == 4u) ? 1.0 : 0.0;
    float a = lerp(ang0, ang1, tip);
    float rad = lerp(r, 0.0, tip);
    float3 p = float3(cos(a) * rad, sin(a) * rad, ubo.quad_z);
    return emitV(p, float3(0, 0, 1));
}

bool dialSegmentActive(float segAngle) {
    uint style = uint(ubo.params.x + 0.5);
    if (style != 4u) {
        return true;
    }
    float a0 = ubo.params.z;
    float a1 = a0 + ubo.params.w;
    if (ubo.params.w >= 0.0) {
        return segAngle >= a0 && segAngle <= a1;
    }
    return segAngle >= a1 && segAngle <= a0;
}

float3 scaleBoxCenter() {
    return ubo.params.w > 0.5 ? float3(0.0, 0.0, 0.0) : float3(0.0, 0.0, 0.8);
}

float scaleBoxHalfExtent() {
    const float width = lineWidthScale();
    const float base = ubo.params.w > 0.5 ? 0.16 : 0.12;
    return base * width;
}

VertexOutput vsScaleBox(uint vid) {
    const float3 c = scaleBoxCenter();
    const float h = scaleBoxHalfExtent();
    const uint face = vid / 6u;
    const float2 uv = quadUV(vid % 6u);

    float3 p = c;
    float3 n = float3(0, 0, 1);
    if (face == 0u) {
        p += float3(lerp(-h, h, uv.x), lerp(-h, h, uv.y), -h);
        n = float3(0, 0, -1);
    } else if (face == 1u) {
        p += float3(lerp(-h, h, uv.x), lerp(-h, h, uv.y), h);
        n = float3(0, 0, 1);
    } else if (face == 2u) {
        p += float3(lerp(-h, h, uv.x), -h, lerp(-h, h, uv.y));
        n = float3(0, -1, 0);
    } else if (face == 3u) {
        p += float3(lerp(-h, h, uv.x), h, lerp(-h, h, uv.y));
        n = float3(0, 1, 0);
    } else if (face == 4u) {
        p += float3(-h, lerp(-h, h, uv.x), lerp(-h, h, uv.y));
        n = float3(-1, 0, 0);
    } else {
        p += float3(h, lerp(-h, h, uv.x), lerp(-h, h, uv.y));
        n = float3(1, 0, 0);
    }
    return emitV(p, n);
}

VertexOutput vsDial(uint vid) {
    const float majorR = 0.82;
    const float tubeW = 0.10 * lineWidthScale();

    uint seg = vid / 6u;
    uint corner = vid % 6u;
    if (seg >= kDialSides) {
        return emitDiscarded();
    }

    float ang0 = float(seg) / float(kDialSides) * 6.2831853;
    float ang1 = float(seg + 1u) / float(kDialSides) * 6.2831853;
    float midAng = (ang0 + ang1) * 0.5;
    if (!dialSegmentActive(midAng)) {
        return emitDiscarded();
    }

    float2 uv = quadUV(corner);
    float a = lerp(ang0, ang1, uv.x);
    float r = lerp(majorR - tubeW, majorR + tubeW, uv.y);
    float3 p = float3(cos(a) * r, sin(a) * r, ubo.quad_z);
    return emitV(p, float3(0, 0, 1));
}

[shader("vertex")]
VertexOutput vertexMain(uint vid : SV_VertexID) {
    uint style = uint(ubo.params.x + 0.5);
    if (style == 0u) {
        if (vid >= kArrowVertCount) {
            return emitDiscarded();
        }
        return vsArrow(vid);
    }
    if (style == 1u) {
        if (vid >= kPlaneVertCount) {
            return emitDiscarded();
        }
        return vsPlane(vid);
    }
    if (style == 2u) {
        if (vid >= kCenterVertCount) {
            return emitDiscarded();
        }
        return vsCenter(vid);
    }
    if (style == 3u || style == 4u) {
        if (vid >= kDialVertCount) {
            return emitDiscarded();
        }
        return vsDial(vid);
    }
    if (style == 5u) {
        if (vid >= kScaleBoxVertCount) {
            return emitDiscarded();
        }
        return vsScaleBox(vid);
    }
    return vsPlane(min(vid, kPlaneVertCount - 1u));
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    if (input.color.a < 0.001) {
        discard;
    }
    float3 lightDir = normalize(float3(0.3, 0.55, 0.9));
    float ndl = saturate(dot(normalize(input.worldNormal), lightDir) * 0.25 + 0.75);
    return float4(input.color.rgb * ndl, input.color.a);
}
