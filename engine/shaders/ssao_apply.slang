struct SsaOUniformData
{
    float4x4 invProjection;
    float4x4 projection;
    float4 clipParams;
    float4 screenParams;
    float4 effectParams; // x=enabled, y=strength, z=power, w=unused
};

[[vk::binding(0, 0)]]
ConstantBuffer<SsaOUniformData> ubo;

[[vk::binding(1, 0)]]
Texture2D<float4> sceneColorTexture;

[[vk::binding(2, 0)]]
SamplerState sceneColorSampler;

[[vk::binding(3, 0)]]
Texture2D<float> aoTexture;

[[vk::binding(4, 0)]]
SamplerState aoSampler;

struct VertexOutput
{
    float4 sv_position : SV_Position;
    [[vk::location(0)]]
    float2 uv;
};

[shader("vertex")]
VertexOutput vertexMain(uint vertexId : SV_VertexID)
{
    VertexOutput output;
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    output.sv_position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    // Interpolated corner UVs are 0 or 2; scale to [0, 1] on the visible triangle.
    output.uv = uv * 0.5;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    // Forward pass already uses Vulkan Y-flipped projection; do not flip again.
    float2 uv = input.uv;
    float4 scene = sceneColorTexture.Sample(sceneColorSampler, uv);

    if (ubo.effectParams.x < 0.5)
    {
        return scene;
    }

    float ao = aoTexture.Sample(aoSampler, uv).r;
    float strength = ubo.effectParams.y;
    float factor = lerp(1.0, ao, strength);
    return float4(scene.rgb * factor, scene.a);
}
