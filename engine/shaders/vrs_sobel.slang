// Graphics-queue Sobel luminance edge → fragment shading rate image.
// Rec.709 luma, 3×3, workgroup 16×16 with 18×18 halo. G > 0.1 → 1×1 else 2×2.
// No Bindless. Exact-match record path: UBO, sampled color, storage rate.

struct VrsSobelUniformData
{
    uint4 sizes; // colorW, colorH, rateW, rateH
    float4 texel; // texelW, texelH, threshold, pad
};

[[vk::binding(0, 0)]]
ConstantBuffer<VrsSobelUniformData> ubo;

[[vk::binding(1, 0)]]
Texture2D colorImage;

[[vk::binding(2, 0)]]
[[vk::image_format("r8ui")]]
RWTexture2D<uint> rateImage;

groupshared float tileLuma[18 * 18];

float rec709(float3 c)
{
    return 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b;
}

float sampleLuma(int2 rateCoord)
{
    int2 texel = int2(int(ubo.texel.x), int(ubo.texel.y));
    int2 colorCoord = rateCoord * texel;
    int maxX = max(int(ubo.sizes.x), 1) - 1;
    int maxY = max(int(ubo.sizes.y), 1) - 1;
    colorCoord.x = clamp(colorCoord.x, 0, maxX);
    colorCoord.y = clamp(colorCoord.y, 0, maxY);
    return rec709(colorImage.Load(int3(colorCoord, 0)).rgb);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 dtid : SV_DispatchThreadID, uint3 gid : SV_GroupID, uint3 gtid : SV_GroupThreadID)
{
    int2 groupBase = int2(gid.xy) * 16 - int2(1, 1);
    uint linear = gtid.y * 16u + gtid.x;
    if (linear < 324u)
    {
        int2 local = int2(int(linear % 18u), int(linear / 18u));
        tileLuma[linear] = sampleLuma(groupBase + local);
    }
    if (linear + 256u < 324u)
    {
        uint idx = linear + 256u;
        int2 local = int2(int(idx % 18u), int(idx / 18u));
        tileLuma[idx] = sampleLuma(groupBase + local);
    }
    GroupMemoryBarrierWithGroupSync();

    if (dtid.x >= ubo.sizes.z || dtid.y >= ubo.sizes.w)
    {
        return;
    }

    int lx = int(gtid.x) + 1;
    int ly = int(gtid.y) + 1;
    float l00 = tileLuma[(ly - 1) * 18 + (lx - 1)];
    float l10 = tileLuma[ly * 18 + (lx - 1)];
    float l20 = tileLuma[(ly + 1) * 18 + (lx - 1)];
    float l01 = tileLuma[(ly - 1) * 18 + lx];
    float l21 = tileLuma[(ly + 1) * 18 + lx];
    float l02 = tileLuma[(ly - 1) * 18 + (lx + 1)];
    float l12 = tileLuma[ly * 18 + (lx + 1)];
    float l22 = tileLuma[(ly + 1) * 18 + (lx + 1)];
    float dx = -l00 - 2.0 * l10 - l20 + l02 + 2.0 * l12 + l22;
    float dy = -l00 - 2.0 * l01 - l02 + l20 + 2.0 * l21 + l22;
    float g = dx * dx + dy * dy;
    uint encoded = g > ubo.texel.z ? 0u : ((1u << 2) | 1u);
    rateImage[dtid.xy] = encoded;
}
