struct OutlinePrepassUniformData
{
    float4x4 model;
    float4x4 view;
    float4x4 projection;
    uint packedObjectId;
    uint3 _pad;
};

[[vk::binding(0, 0)]]
ConstantBuffer<OutlinePrepassUniformData> ubo;

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
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;
    float4 worldPos = mul(ubo.model, float4(input.inPosition, 1.0));
    float4 viewPos = mul(ubo.view, worldPos);
    output.sv_position = mul(ubo.projection, viewPos);
    return output;
}

[shader("fragment")]
uint fragmentMain(VertexOutput input) : SV_Target
{
    return ubo.packedObjectId;
}
