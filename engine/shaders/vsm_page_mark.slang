struct PageMarkUniform
{
    float4x4 invViewProjection;
    float4x4 lightView;
    float4 originLevels; // xyz unused (origin is in lightView), w firstLevel
    float4 vsmParams;    // lastLevel, pagesPerAxis, 0, enabled
    float4 depthSize;    // width, height, 0, 0
};

[[vk::binding(0, 0)]]
ConstantBuffer<PageMarkUniform> ubo;

[[vk::binding(1, 0)]]
Texture2D<float> depthTexture;

[[vk::binding(2, 0)]]
RWStructuredBuffer<uint> pageFlags;

float vsmClipmapRadius(uint level)
{
    return exp2(float(level + 1u));
}

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint width = uint(ubo.depthSize.x);
    uint height = uint(ubo.depthSize.y);
    if (dtid.x >= width || dtid.y >= height || ubo.vsmParams.w < 0.5)
    {
        return;
    }
    // D32 views: SampleLevel/Gather are undefined or VK_ERROR_DEVICE_LOST on
    // Intel ICDs (same as hiz_pyramid mip 0). Texel Load is defined.
    int2 p = int2(dtid.xy);
    float depth = depthTexture.Load(int3(p, 0));
    if (depth >= 1.0)
    {
        return;
    }
    float2 uv = (float2(dtid.xy) + 0.5) / float2(width, height);
    float4 clip = float4(uv * 2.0 - 1.0, depth, 1.0);
    float4 world = mul(ubo.invViewProjection, clip);
    world.xyz /= world.w;
    float3 lightPos = mul(ubo.lightView, float4(world.xyz, 1.0)).xyz;
    uint firstLevel = uint(ubo.originLevels.w + 0.5);
    uint lastLevel = uint(ubo.vsmParams.x + 0.5);
    uint pagesPerAxis = uint(ubo.vsmParams.y + 0.5);
    if (pagesPerAxis == 0u)
    {
        pagesPerAxis = 128u;
    }
    for (uint level = firstLevel; level <= lastLevel; ++level)
    {
        float radius = vsmClipmapRadius(level);
        float2 pageUv = lightPos.xy / (2.0 * radius) + 0.5;
        if (pageUv.x < 0.0 || pageUv.x >= 1.0 || pageUv.y < 0.0 || pageUv.y >= 1.0)
        {
            continue;
        }
        uint pageX = uint(pageUv.x * float(pagesPerAxis));
        uint pageY = uint(pageUv.y * float(pagesPerAxis));
        uint local = level - firstLevel;
        uint virt = local * pagesPerAxis * pagesPerAxis + pageY * pagesPerAxis + pageX;
        if (virt >= 81920u)
        {
            continue;
        }
        InterlockedOr(pageFlags[virt], 1u);
    }
}
