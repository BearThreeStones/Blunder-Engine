static const float3 kObjectSelectOrange = float3(0.960784f, 0.439216f, 0.066667f);
static const float3 kTransformOrange = float3(0.96f, 0.44f, 0.07f);
static const float3 kTransformActiveLight = float3(0.92f, 0.92f, 0.94f);
static const float kAlphaOccluded = 0.35f;
static const float kDepthEpsilon = 3.0f / 8388608.0f;
static const float kMaxOutlineAlpha = 254.0f / 255.0f;

struct OutlineResolveUniformData
{
    float4 screenParams; // xy = 1/size, zw = size
};

[[vk::binding(0, 0)]]
ConstantBuffer<OutlineResolveUniformData> ubo;

[[vk::binding(1, 0)]]
Texture2D<uint> objectIdTexture;

[[vk::binding(2, 0)]]
Texture2D<float> outlineDepthTexture;

[[vk::binding(3, 0)]]
Texture2D<float> sceneDepthTexture;

[[vk::binding(4, 0)]]
SamplerState depthSampler;

struct VertexOutput
{
    float4 sv_position : SV_Position;
};

[shader("vertex")]
VertexOutput vertexMain(uint vertexId : SV_VertexID)
{
    VertexOutput output;
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    output.sv_position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    return output;
}

uint getColorId(uint packed)
{
    return packed >> 14u;
}

bool isEdgeNeighbor(uint center, uint neighbor)
{
    if (center == 0u)
    {
        return false;
    }
    if (neighbor == 0u)
    {
        return true;
    }
    if (getColorId(neighbor) != getColorId(center))
    {
        return true;
    }
    return false;
}

uint loadObjectId(int2 pixel)
{
    if (pixel.x < 0 || pixel.y < 0)
    {
        return 0u;
    }
    if (pixel.x >= int(ubo.screenParams.z) || pixel.y >= int(ubo.screenParams.w))
    {
        return 0u;
    }
    return objectIdTexture.Load(int3(pixel, 0));
}

static const float kEdgeSmoothMin = 0.25;
static const float kEdgeSmoothMax = 2.5;

float edgeCoverage(uint refId, int2 pixel)
{
    if (refId == 0u)
    {
        return 0.0;
    }

    static const int2 kOffsets[8] = {
        int2(1, 0), int2(-1, 0), int2(0, 1), int2(0, -1),
        int2(1, 1), int2(-1, 1), int2(1, -1), int2(-1, -1)};

    float weight = 0.0;
    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        const uint neighborId = loadObjectId(pixel + kOffsets[i]);
        if (isEdgeNeighbor(refId, neighborId))
        {
            weight += 1.0;
        }
    }
    float t = (weight - kEdgeSmoothMin) / max(kEdgeSmoothMax - kEdgeSmoothMin, 1e-4);
    return saturate(t);
}

float3 outlineColor(uint colorId)
{
    if (colorId == 0u)
    {
        return kTransformOrange;
    }
    if (colorId == 2u)
    {
        return kTransformActiveLight;
    }
    return kObjectSelectOrange;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    int2 pixel = int2(input.sv_position.xy);
    const uint refId = loadObjectId(pixel);
    if (refId == 0u)
    {
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    const float coverage = edgeCoverage(refId, pixel);
    if (coverage <= 0.0)
    {
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    float2 depthUv = (float2(pixel) + 0.5) * ubo.screenParams.xy;
    float outlineDepth = outlineDepthTexture.Sample(depthSampler, depthUv).r;
    float sceneDepth = sceneDepthTexture.Sample(depthSampler, depthUv).r;
    bool occluded = outlineDepth > sceneDepth + kDepthEpsilon;
    float alpha = (occluded ? kAlphaOccluded : 1.0f) * kMaxOutlineAlpha * coverage;
    return float4(outlineColor(getColorId(refId)), alpha);
}
