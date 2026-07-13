// transform_gizmo.slang — translate handles + rotate dials + ghost arc
//
// Plane quads (style 1) parameterize position/size via quad_layout.
// Arrows (style 0): navigate-style view-space arm quad + 8-sided cone tip along +Z.

static const uint kArrowStemVertCount = 6u;
static const uint kArrowConeSides = 8u;
static const uint kArrowConeVertCount = kArrowConeSides * 6u;
static const uint kArrowVertCount = kArrowStemVertCount + kArrowConeVertCount;
static const uint kPlaneVertCount = 6u;
static const uint kCenterVertCount = kSdfRingVertCount;
static const uint kScaleBoxVertCount = 36u;
static const uint kScaleStemVertCount = 6u;
static const uint kScaleStemBoxVertCount = kScaleStemVertCount + kScaleBoxVertCount;

// Must match TransformGizmoMetrics in transform_gizmo_types.h
static const float kMeshStemHalfWidth = 0.025;
static const float kMeshStemLength = 0.72;
static const float kMeshConeBaseZ = 0.72;
static const float kMeshArrowLength = 1.0;
static const float kMeshConeBaseRadius = 0.06;
static const float kMeshCenterRadius = 0.20;
static const float kMeshDialMajorRadius = 1.0;
static const float kMeshOuterRingRadius = 1.0;
static const float kArcInnerFactor = 6.0;
static const float kMeshScaleStemStart = 0.5;
static const float kMeshScaleBoxOffset = 0.8;
static const float kMeshScaleBoxHalfExtent = 0.10;
static const float kMeshScaleCenterHalfExtent = 0.12;
static const float kPolylineSmoothWidth = 1.0;
static const float kPlaneInteriorAlphaFactor = 0.45;
static const uint kSdfRingVertCount = 6u;
static const uint kDialVertCount = kSdfRingVertCount;
static const uint kDialWireVertCount = kSdfRingVertCount;
static const uint kAnnulusVertCount = kSdfRingVertCount * 2u;
static const uint kDialFillVertCount = kSdfRingVertCount;
// strokeCoord markers: -2 SDF ring; -3 SDF filled disc; -5 SDF rect border; >1 solid mesh/arm
static const float kStrokeSdfRing = -2.0;
static const float kStrokeSdfDisc = -3.0;
static const float kStrokeSdfRect = -5.0;

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
    float clip_enabled;
    float line_width_px;
    float viewport_height_px;
    float4 clip_plane;
    float dial_sides;
};

[[vk::binding(0, 0)]]
ConstantBuffer<TransformGizmoUniform> ubo;

struct VertexOutput {
    float4 sv_position : SV_Position;
    [[vk::location(0)]]
    float3 worldNormal;
    [[vk::location(1)]]
    float4 color;
    [[vk::location(2)]]
    float3 worldPos;
    [[vk::location(3)]]
    float strokeCoord;
    [[vk::location(4)]]
    float2 ringLocalXY;
    [[vk::location(5)]]
    float ringMajorR;
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

float3 worldFromLocal(float3 localPos) {
    return mul(ubo.gizmoWorld, float4(localPos, 1.0)).xyz;
}

float lineWidthScale() {
    uint style = uint(ubo.params.x + 0.5);
    if (style == 4u) {
        return max(ubo.params.z, 0.5);
    }
    if (style == 0u || style == 5u || style == 9u) {
        return max(ubo.params.z, 0.5);
    }
    return max(ubo.params.z, 0.5);
}

float localUnitsPerPixel(float2 local_xy) {
    float2 dx = ddx(local_xy);
    float2 dy = ddy(local_xy);
    return max(0.5 * (length(dx) + length(dy)), 1e-6);
}

float sdfStrokeHalfW() {
    return max(ubo.line_width_px * 0.5, 0.1);
}

float ringLineAlpha(float2 local_xy, float radius) {
    float sdf = abs(length(local_xy) - radius);
    float halfW = max(ubo.line_width_px * 0.5, 0.75);
    float pxDistJacobian = sdf / localUnitsPerPixel(local_xy);
    float aJacobian = saturate(halfW + 1.25 - pxDistJacobian);
    float pxDistFw = sdf / max(fwidth(sdf), localUnitsPerPixel(local_xy) * 1e-3);
    float aFw = saturate(halfW + 1.25 - pxDistFw);
    return max(aJacobian, aFw);
}

float sdBox2d(float2 p, float2 half_ext) {
    float2 d = abs(p) - half_ext;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

float rectRingAlpha(float2 local_xy, float2 half_ext) {
    float sdf = abs(sdBox2d(local_xy, half_ext));
    float halfW = sdfStrokeHalfW();
    float pxDistJacobian = sdf / localUnitsPerPixel(local_xy);
    float aJacobian = saturate(halfW + 1.25 - pxDistJacobian);
    float pxDistFw = sdf / max(fwidth(sdf), localUnitsPerPixel(local_xy) * 1e-3);
    float aFw = saturate(halfW + 1.25 - pxDistFw);
    return max(aJacobian, aFw);
}

float rectFillAlpha(float2 local_xy, float2 half_ext) {
    float sdf = sdBox2d(local_xy, half_ext);
    float pxDist = sdf / localUnitsPerPixel(local_xy);
    return saturate(0.75 - pxDist);
}

float discFillAlpha(float2 local_xy, float radius) {
    float sdf = length(local_xy) - radius;
    float pxDist = sdf / localUnitsPerPixel(local_xy);
    return saturate(0.75 - pxDist);
}

float discFillAlphaSolid(float2 local_xy, float radius) {
    float sdf = length(local_xy) - radius;
    float pxDist = sdf / localUnitsPerPixel(local_xy);
    return saturate(1.25 - pxDist);
}

float2 getArmParams(uint idx) {
    switch (idx) {
        case 0: return float2(0, -1);
        case 1: return float2(1, -1);
        case 2: return float2(1,  1);
        case 3: return float2(0, -1);
        case 4: return float2(1,  1);
        default: return float2(0,  1);
    }
}

float3 worldToView(float3 worldPos) {
    return mul(ubo.view, float4(worldPos, 1.0)).xyz;
}

// Navigate-gizmo-style arm: view-space extrusion perpendicular to projected stem (primType 2).
VertexOutput vsNavigateStyleArmQuad(uint vid, float3 local_a, float3 local_b) {
    float3 startWorld = worldFromLocal(local_a);
    float3 endWorld = worldFromLocal(local_b);
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
    o.worldNormal = normalFromLocal(float3(0, 1, 0));
    o.color = float4(ubo.color.rgb, ubo.color.a * ubo.params.y);
    o.worldPos = lerp(startWorld, endWorld, ap.x);
    o.strokeCoord = 2.0;
    o.ringLocalXY = float2(0.0, 0.0);
    o.ringMajorR = 0.0;
    return o;
}

float4 clipFromLocal(float3 localPos) {
    return mul(ubo.proj, mul(ubo.view, mul(ubo.gizmoWorld, float4(localPos, 1.0))));
}

VertexOutput emitV(float3 p, float3 n) {
    VertexOutput o;
    o.sv_position = projectLocal(p);
    o.worldNormal = normalFromLocal(n);
    o.color = float4(ubo.color.rgb, ubo.color.a * ubo.params.y);
    o.worldPos = worldFromLocal(p);
    o.strokeCoord = 2.0;
    o.ringLocalXY = float2(0.0, 0.0);
    o.ringMajorR = 0.0;
    return o;
}

VertexOutput emitDiscarded() {
    VertexOutput o;
    o.sv_position = float4(0, 0, 2, 1);
    o.worldNormal = float3(0, 0, 1);
    o.color = float4(0, 0, 0, 0);
    o.worldPos = float3(0, 0, 0);
    o.strokeCoord = 2.0;
    o.ringLocalXY = float2(0.0, 0.0);
    o.ringMajorR = 0.0;
    return o;
}

VertexOutput vsSdfRingQuad(uint vid, float majorR, float strokeMarker, float pad_scale) {
    float4 clipC = clipFromLocal(float3(0.0, 0.0, ubo.quad_z));
    float pxPad = ubo.line_width_px + kPolylineSmoothWidth + 2.0;
    float padLocal = pxPad * 0.5 * (2.0 / max(ubo.viewport_height_px, 1.0)) * abs(clipC.w) * pad_scale;
    float extR = majorR + padLocal;

    float2 uv = quadUV(vid);
    float2 p = float2(lerp(-extR, extR, uv.x), lerp(-extR, extR, uv.y));

    VertexOutput o;
    o.sv_position = projectLocal(float3(p, ubo.quad_z));
    o.worldNormal = normalFromLocal(float3(0, 0, 1));
    o.color = float4(ubo.color.rgb, ubo.color.a * ubo.params.y);
    o.worldPos = worldFromLocal(float3(p, ubo.quad_z));
    o.strokeCoord = strokeMarker;
    o.ringLocalXY = p;
    o.ringMajorR = majorR;
    return o;
}

VertexOutput vsSdfRectQuad(uint vid, float pad_scale) {
    float cx = ubo.quad_layout.x;
    float cy = ubo.quad_layout.y;
    float hx = ubo.quad_layout.z;
    float hy = ubo.quad_layout.w;
    float4 clipC = clipFromLocal(float3(cx, cy, ubo.quad_z));
    float pxPad = ubo.line_width_px + kPolylineSmoothWidth + 2.0;
    float padLocal = pxPad * 0.5 * (2.0 / max(ubo.viewport_height_px, 1.0)) * abs(clipC.w) * pad_scale;
    float extX = hx + padLocal;
    float extY = hy + padLocal;

    float2 uv = quadUV(vid);
    float2 p = float2(lerp(cx - extX, cx + extX, uv.x), lerp(cy - extY, cy + extY, uv.y));

    VertexOutput o;
    o.sv_position = projectLocal(float3(p, ubo.quad_z));
    o.worldNormal = normalFromLocal(float3(0, 0, 1));
    o.color = float4(ubo.color.rgb, ubo.color.a * ubo.params.y);
    o.worldPos = worldFromLocal(float3(p, ubo.quad_z));
    o.strokeCoord = kStrokeSdfRect;
    o.ringLocalXY = p;
    o.ringMajorR = 0.0;
    return o;
}

// Thin stem quad in local XZ (+Y normal) plus 8-sided cone tip at +Z end.
VertexOutput vsArrow(uint vid) {
    const float width = lineWidthScale();
    const float stemLen = kMeshStemLength;
    const float coneBaseZ = kMeshConeBaseZ;
    const float coneTipZ = kMeshArrowLength;
    const float coneBaseR = kMeshConeBaseRadius * width;

    if (vid < kArrowStemVertCount) {
        return vsNavigateStyleArmQuad(vid, float3(0.0, 0.0, 0.0), float3(0.0, 0.0, stemLen));
    }

    uint coneVid = vid - kArrowStemVertCount;
    uint side = coneVid / 6u;
    uint corner = coneVid % 6u;
    if (side >= kArrowConeSides) {
        return emitDiscarded();
    }

    float ang0 = float(side) / float(kArrowConeSides) * 6.2831853;
    float ang1 = float(side + 1u) / float(kArrowConeSides) * 6.2831853;
    float2 uv = quadUV(corner);
    float ang = lerp(ang0, ang1, uv.x);
    float t = uv.y;
    float r = lerp(coneBaseR, 0.0, t);
    float z = lerp(coneBaseZ, coneTipZ, t);
    float3 p = float3(cos(ang) * r, sin(ang) * r, z);
    float3 n = normalize(float3(cos(ang), sin(ang), 0.25));
    return emitV(p, n);
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
    return ubo.params.w > 0.5 ? float3(0.0, 0.0, 0.0) : float3(0.0, 0.0, kMeshScaleBoxOffset);
}

float scaleBoxHalfExtent() {
    const float width = lineWidthScale();
    const float base = ubo.params.w > 0.5 ? kMeshScaleCenterHalfExtent : kMeshScaleBoxHalfExtent;
    return base * width;
}

VertexOutput vsScaleBox(uint vid, float3 centerOverride) {
    const float3 c = centerOverride;
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

VertexOutput vsScaleBox(uint vid) {
    return vsScaleBox(vid, scaleBoxCenter());
}

VertexOutput vsScaleStemBox(uint vid) {
    if (vid < kScaleStemVertCount) {
        return vsNavigateStyleArmQuad(vid, float3(0.0, 0.0, kMeshScaleStemStart),
                                      float3(0.0, 0.0, kMeshScaleBoxOffset));
    }

    return vsScaleBox(vid - kScaleStemVertCount, float3(0.0, 0.0, kMeshScaleBoxOffset));
}

VertexOutput vsDialFill(uint vid) {
    if (vid >= kDialFillVertCount) {
        return emitDiscarded();
    }
    return vsSdfRingQuad(vid, kMeshOuterRingRadius, kStrokeSdfDisc, 1.0);
}

VertexOutput vsAnnulus(uint vid) {
    if (vid < kSdfRingVertCount) {
        return vsSdfRingQuad(vid, kMeshOuterRingRadius, kStrokeSdfRing, 1.0);
    }
    if (vid < kAnnulusVertCount) {
        return vsSdfRingQuad(vid - kSdfRingVertCount, kMeshOuterRingRadius / kArcInnerFactor,
                             kStrokeSdfRing, 1.0);
    }
    return emitDiscarded();
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
        return vsSdfRectQuad(vid, 1.0);
    }
    if (style == 2u) {
        if (vid >= kCenterVertCount) {
            return emitDiscarded();
        }
        const float r = kMeshCenterRadius * lineWidthScale();
        return vsSdfRingQuad(vid, r, kStrokeSdfDisc, 1.0);
    }
    if (style == 3u || style == 4u) {
        if (vid >= kDialVertCount) {
            return emitDiscarded();
        }
        return vsSdfRingQuad(vid, kMeshDialMajorRadius, kStrokeSdfRing, 1.0);
    }
    if (style == 5u) {
        if (vid >= kScaleBoxVertCount) {
            return emitDiscarded();
        }
        return vsScaleBox(vid);
    }
    if (style == 6u) {
        if (vid >= kDialWireVertCount) {
            return emitDiscarded();
        }
        return vsSdfRingQuad(vid, kMeshOuterRingRadius, kStrokeSdfRing, 2.5);
    }
    if (style == 7u) {
        if (vid >= kAnnulusVertCount) {
            return emitDiscarded();
        }
        return vsAnnulus(vid);
    }
    if (style == 8u) {
        if (vid >= kDialFillVertCount) {
            return emitDiscarded();
        }
        return vsDialFill(vid);
    }
    if (style == 9u) {
        if (vid >= kScaleStemBoxVertCount) {
            return emitDiscarded();
        }
        return vsScaleStemBox(vid);
    }
    return emitDiscarded();
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    if (input.color.a < 0.001) {
        discard;
    }

    const bool isSdfPath = input.strokeCoord < -1.5;
    float3 clipWorldPos = input.worldPos;
    if (isSdfPath) {
        // Quad worldPos is perspective-interpolated; rebuild from local ring coords for stable clip.
        clipWorldPos = mul(ubo.gizmoWorld, float4(input.ringLocalXY, ubo.quad_z, 1.0)).xyz;
    }
    if (ubo.clip_enabled > 0.5) {
        float clip_dist = dot(ubo.clip_plane.xyz, clipWorldPos) + ubo.clip_plane.w;
        if (clip_dist < 0.0) {
            discard;
        }
    }

    float4 color = input.color;

    if (input.strokeCoord < -4.5) {
        float2 center = ubo.quad_layout.xy;
        float2 half_ext = ubo.quad_layout.zw;
        float2 rel = input.ringLocalXY - center;
        float border = rectRingAlpha(rel, half_ext);
        float fill = rectFillAlpha(rel, half_ext);
        float fade = ubo.params.y;
        float interior_a = input.color.a * kPlaneInteriorAlphaFactor;
        float border_a = fade;
        color.a = max(border * border_a, fill * interior_a);
        if (color.a < 1e-4) {
            discard;
        }
        return color;
    }

    if (input.strokeCoord < -2.5) {
        uint style = uint(ubo.params.x + 0.5);
        if (style == 2u) {
            color.a *= discFillAlphaSolid(input.ringLocalXY, input.ringMajorR);
        } else {
            color.a *= discFillAlpha(input.ringLocalXY, input.ringMajorR);
        }
        if (color.a < 0.001) {
            discard;
        }
        return color;
    }

    if (input.strokeCoord < -1.5) {
        uint style = uint(ubo.params.x + 0.5);
        if (style == 4u) {
            float ang = atan2(input.ringLocalXY.y, input.ringLocalXY.x);
            if (ang < 0.0) {
                ang += 6.2831853;
            }
            if (!dialSegmentActive(ang)) {
                discard;
            }
        }
        color.a *= ringLineAlpha(input.ringLocalXY, input.ringMajorR);
        if (color.a < 1e-4) {
            discard;
        }
        return color;
    }

    return color;
}
