// Editor Viewport debug blit of the fragment shading rate image.
// 1×1 (texel 0) dark; 2×2 red. Packt Figure 9.4. Not Player / Preview.

struct VrsRateMaskUniformData
{
    uint4 sizes; // colorW, colorH, rateW, rateH
    float4 texel; // texelW, texelH, pad
};

[[vk::binding(0, 0)]]
ConstantBuffer<VrsRateMaskUniformData> ubo;

[[vk::binding(1, 0)]]
Texture2D<uint> rateImage;

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

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    int2 texelSize = int2(max(int(ubo.texel.x), 1), max(int(ubo.texel.y), 1));
    int2 rateCoord = int2(input.sv_position.xy) / texelSize;
    int maxX = max(int(ubo.sizes.z), 1) - 1;
    int maxY = max(int(ubo.sizes.w), 1) - 1;
    rateCoord.x = clamp(rateCoord.x, 0, maxX);
    rateCoord.y = clamp(rateCoord.y, 0, maxY);
    uint rate = rateImage.Load(int3(rateCoord, 0));
    if (rate == 0u)
    {
        return float4(0.04, 0.04, 0.07, 1.0);
    }
    return float4(0.85, 0.12, 0.08, 1.0);
}
