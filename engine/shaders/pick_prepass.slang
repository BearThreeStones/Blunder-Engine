struct PickPrepassUniformData
{
    float4x4 model;
    float4x4 view;
    float4x4 projection;
    uint entityId;
    uint alphaMode;
    float alphaCutoff;
    float hasBaseColorTexture;
    float4 baseColorFactor;
    uint isPeelPass;
    float peelDepth;
    float peelEpsilon;
};

[[vk::binding(0, 0)]]
ConstantBuffer<PickPrepassUniformData> ubo;

[[vk::binding(1, 0)]]
Texture2D<float4> baseColorTexture;

[[vk::binding(2, 0)]]
SamplerState baseColorSampler;

struct VertexInput
{
    [[vk::location(0)]]
    float3 inPosition;
    [[vk::location(1)]]
    float3 inNormal;
    [[vk::location(2)]]
    float2 inUv;
};

struct VertexOutput
{
    float4 sv_position : SV_Position;
    [[vk::location(0)]]
    float2 uv;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;
    float4 worldPos = mul(ubo.model, float4(input.inPosition, 1.0));
    float4 viewPos = mul(ubo.view, worldPos);
    output.sv_position = mul(ubo.projection, viewPos);
    output.uv = input.inUv;
    return output;
}

[shader("fragment")]
uint fragmentMain(VertexOutput input) : SV_Target
{
    // perspectiveZO: nearer surfaces have greater depth [0,1] (near=1, far=0).
    if (ubo.isPeelPass != 0u)
    {
        if (input.sv_position.z >= ubo.peelDepth - ubo.peelEpsilon)
        {
            discard;
        }
    }

    if (ubo.alphaMode == 1u)
    {
        float alpha = ubo.baseColorFactor.a;
        if (ubo.hasBaseColorTexture > 0.5)
        {
            alpha *= baseColorTexture.Sample(baseColorSampler, input.uv).a;
        }
        clip(alpha - ubo.alphaCutoff);
    }
    return ubo.entityId;
}
