// transform_gizmo.slang — translate handles + rotate dials + ghost arc

static const uint kConeSides = 12u;
static const uint kConeVertCount = kConeSides * 6u;
static const uint kStemSides = 6u;
static const uint kStemVertCount = kStemSides * 6u;
static const uint kCenterSides = 20u;
static const uint kCenterVertCount = kCenterSides * 6u;

static const uint kDialSides = 48u;
static const uint kDialTubeSides = 6u;
static const uint kDialVertCount = kDialSides * kDialTubeSides * 6u;

struct TransformGizmoUniform {
    float4x4 view;
    float4x4 proj;
    float4x4 gizmoWorld;
    float4 color;
    // x=style, y=alpha, z=arc_start_rad, w=arc_delta_rad
    float4 params;
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

VertexOutput emitV(float3 p, float3 n) {
    VertexOutput o;
    o.sv_position = projectLocal(p);
    o.worldNormal = normalFromLocal(n);
    o.color = float4(ubo.color.rgb, ubo.color.a * ubo.params.y);
    return o;
}

VertexOutput vsArrow(uint vid) {
    const float stemLen = 0.7;
    const float coneLen = 0.3;
    const float stemR = 0.04;
    const float coneR = 0.12;

    if (vid < kStemVertCount) {
        uint side = vid / 6u;
        uint corner = vid % 6u;
        float ang = float(side) / float(kStemSides) * 6.2831853;
        float ang2 = float(side + 1u) / float(kStemSides) * 6.2831853;
        float2 uv = quadUV(corner);
        float useB = (corner >= 3u) ? 1.0 : 0.0;
        float a = lerp(ang, ang2, useB);
        float z = lerp(0.0, stemLen, uv.y);
        float r = stemR;
        float3 p = float3(cos(a) * r, sin(a) * r, z);
        float3 n = normalize(float3(cos(a), sin(a), 0.0));
        return emitV(p, n);
    }

    uint cv = vid - kStemVertCount;
    uint side = cv / 6u;
    uint corner = cv % 6u;
    float ang = float(side) / float(kConeSides) * 6.2831853;
    float ang2 = float(side + 1u) / float(kConeSides) * 6.2831853;
    float tip = (corner == 2u || corner == 4u) ? 1.0 : 0.0;
    float a = lerp(ang, ang2, tip);
    float z = stemLen + coneLen * tip;
    float r = lerp(coneR, 0.0, tip);
    float3 p = float3(cos(a) * r, sin(a) * r, z);
    float3 n = normalize(float3(cos(a), sin(a), 0.25));
    return emitV(p, n);
}

VertexOutput vsPlane(uint vid) {
    const float h = 0.32;
    float2 uv = quadUV(vid);
    float3 p = float3(lerp(-h, h, uv.x), lerp(-h, h, uv.y), 0.0);
    return emitV(p, float3(0, 0, 1));
}

VertexOutput vsCenter(uint vid) {
    const float r = 0.2;
    uint side = vid / 6u;
    uint corner = vid % 6u;
    float ang0 = float(side) / float(kCenterSides) * 6.2831853;
    float ang1 = float(side + 1u) / float(kCenterSides) * 6.2831853;
    float tip = (corner == 2u || corner == 4u) ? 1.0 : 0.0;
    float a = lerp(ang0, ang1, tip);
    float rad = lerp(r, 0.0, tip);
    float3 p = float3(cos(a) * rad, sin(a) * rad, 0.0);
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

VertexOutput vsDial(uint vid) {
    const float majorR = 0.82;
    const float tubeR = 0.045;

    uint seg = vid / (kDialTubeSides * 6u);
    uint rem = vid % (kDialTubeSides * 6u);
    uint tube = rem / 6u;
    uint corner = rem % 6u;

    float segAng0 = float(seg) / float(kDialSides) * 6.2831853;
    float segAng1 = float(seg + 1u) / float(kDialSides) * 6.2831853;
    float midAng = (segAng0 + segAng1) * 0.5;

    if (!dialSegmentActive(midAng)) {
        VertexOutput o;
        o.sv_position = float4(0, 0, 0, 1);
        o.worldNormal = float3(0, 0, 1);
        o.color = float4(0, 0, 0, 0);
        return o;
    }

    float tubeAng0 = float(tube) / float(kDialTubeSides) * 6.2831853;
    float tubeAng1 = float(tube + 1u) / float(kDialTubeSides) * 6.2831853;
    float2 uv = quadUV(corner);
    float useB = (corner >= 3u) ? 1.0 : 0.0;

    float sa = lerp(segAng0, segAng1, useB);
    float ta = lerp(tubeAng0, tubeAng1, useB);

    float3 ringCenter = float3(cos(sa) * majorR, sin(sa) * majorR, 0.0);
    float3 radial = normalize(float3(cos(sa), sin(sa), 0.0));
    float3 tangent = float3(-sin(sa), cos(sa), 0.0);
    float3 p = ringCenter + radial * (cos(ta) * tubeR) + tangent * (sin(ta) * tubeR * 0.35);
    float3 n = normalize(radial * cos(ta) + float3(0, 0, 1) * sin(ta));
    return emitV(p, n);
}

[shader("vertex")]
VertexOutput vertexMain(uint vid : SV_VertexID) {
    uint style = uint(ubo.params.x + 0.5);
    if (style == 0u) {
        return vsArrow(vid);
    }
    if (style == 1u) {
        return vsPlane(vid);
    }
    if (style == 2u) {
        return vsCenter(vid);
    }
    if (style == 3u || style == 4u) {
        if (vid >= kDialVertCount) {
            return emitV(float3(0, 0, 0), float3(0, 0, 1));
        }
        return vsDial(vid);
    }
    return vsCenter(vid);
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    if (input.color.a < 0.001) {
        discard;
    }
    float3 lightDir = normalize(float3(0.3, 0.55, 0.9));
    float ndl = saturate(dot(normalize(input.worldNormal), lightDir) * 0.6 + 0.4);
    return float4(input.color.rgb * ndl, input.color.a);
}
