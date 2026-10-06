// debug_draw.slang — pixel-width debug lines with x-ray alpha.

static const float kPolylineSmoothWidth = 0.5;
static const float kLineEndPadPx = 1.0;
static const uint kLineVertCount = 6u;

struct DebugDrawUniform {
    float4x4 view;
    float4x4 proj;
    float line_width_px;
    float viewport_width_px;
    float viewport_height_px;
    float command_count;
    float alpha_mul;
    float pad0;
    float pad1;
    float pad2;
};

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
ConstantBuffer<DebugDrawUniform> ubo;

[[vk::binding(1, 0)]]
StructuredBuffer<GizmoLineCommand> commands;

struct VertexOutput {
    float4 sv_position : SV_Position;
    [[vk::location(0)]]
    float4 color;
    [[vk::location(1)]]
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

VertexOutput emitDiscarded() {
    VertexOutput o;
    o.sv_position = float4(0, 0, 2, 1);
    o.color = float4(0, 0, 0, 0);
    o.disc_uv = float2(0, 0);
    return o;
}

float4 worldToClip(float3 w) {
    return mul(ubo.proj, mul(ubo.view, float4(w, 1.0)));
}

[shader("vertex")]
VertexOutput vertexMain(uint vid : SV_VertexID, uint iid : SV_InstanceID) {
    uint count = uint(ubo.command_count + 0.5);
    if (iid >= count || vid >= kLineVertCount) {
        return emitDiscarded();
    }
    GizmoLineCommand cmd = commands[iid];
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
    float width = cmd.width_px > 0.0 ? cmd.width_px : ubo.line_width_px;
    float pixelWidth = width + kPolylineSmoothWidth;
    float4 outpos = (Mv / Mv.w) + float4(screenNormal * ap.y * pixelWidth * 0.5, 0, 0);
    float forwards = (ap.x - 0.5) * 2.0;
    outpos.xy += forwards * (pixelDelta / vp) * 0.5 * kLineEndPadPx;
    outpos *= Mv.w;

    VertexOutput o;
    o.sv_position = outpos;
    o.color = cmd.color;
    o.color.a *= ubo.alpha_mul;
    o.disc_uv = float2(ap.y * 0.5 + 0.5, ap.x);
    return o;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    float4 color = input.color;
    if (color.a < 0.001) {
        discard;
    }
    float dist = abs((input.disc_uv.x - 0.5) * 2.0);
    float aa = max(fwidth(dist), 1e-4);
    color.a *= saturate((1.0 - dist) / aa);
    if (color.a < 0.001) {
        discard;
    }
    return color;
}
