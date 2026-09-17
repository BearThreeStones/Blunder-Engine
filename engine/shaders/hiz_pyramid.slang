struct HizUniforms
{
    uint srcWidth;
    uint srcHeight;
    uint dstWidth;
    uint dstHeight;
    uint srcIsDepth;
    uint pad0;
    uint pad1;
    uint pad2;
};

[[vk::binding(0, 0)]]
ConstantBuffer<HizUniforms> ubo;

[[vk::binding(1, 0)]]
Texture2D<float> srcTexture;

[[vk::binding(2, 0)]]
SamplerState srcSampler;

[[vk::binding(3, 0)]]
RWTexture2D<float> dstTexture;

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= ubo.dstWidth || id.y >= ubo.dstHeight)
    {
        return;
    }

    float4 samples;
    if (ubo.srcIsDepth != 0u)
    {
        // Depth views: OpImageGather on a D32 sampled image is invalid SPIR-V
        // (must be Dref) and VK_ERROR_DEVICE_LOST (-4) on Intel ICDs. SampleLevel
        // can return 0 on depth. Texel Load is defined and keeps the MAX reduce.
        int2 srcSize = int2(max(int(ubo.srcWidth), 1), max(int(ubo.srcHeight), 1));
        int2 p0 = int2(id.xy);
        int2 p1 = min(p0 + int2(1, 0), srcSize - 1);
        int2 p2 = min(p0 + int2(0, 1), srcSize - 1);
        int2 p3 = min(p0 + int2(1, 1), srcSize - 1);
        samples = float4(
            srcTexture.Load(int3(p0, 0)),
            srcTexture.Load(int3(p1, 0)),
            srcTexture.Load(int3(p2, 0)),
            srcTexture.Load(int3(p3, 0)));
    }
    else
    {
        float2 uv = (float2(id.xy) + 0.5) / float2(ubo.dstWidth, ubo.dstHeight);
        float2 texel = 1.0 / float2(max(ubo.srcWidth, 1u), max(ubo.srcHeight, 1u));
        samples = float4(
            srcTexture.SampleLevel(srcSampler, uv + float2(-texel.x, -texel.y), 0.0),
            srcTexture.SampleLevel(srcSampler, uv + float2(texel.x, -texel.y), 0.0),
            srcTexture.SampleLevel(srcSampler, uv + float2(-texel.x, texel.y), 0.0),
            srcTexture.SampleLevel(srcSampler, uv + float2(texel.x, texel.y), 0.0));
    }
    // LESS-Z: keep the farthest closest-surface so a covering mip is conservative.
    dstTexture[id.xy] = max(max(samples.x, samples.y), max(samples.z, samples.w));
}
