struct ShadowUniformData
{
    float4x4 model;
    float4x4 lightView;
    float4x4 lightProjection;
};

[[vk::binding(0, 0)]]
ConstantBuffer<ShadowUniformData> ubo;

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
    float4 lightViewPos = mul(ubo.lightView, worldPos);
    output.sv_position = mul(ubo.lightProjection, lightViewPos);
    return output;
}

[shader("fragment")]
void fragmentMain(VertexOutput input)
{
}
