struct GlobalUniformData
{
    float4x4 model;
    float4x4 view;
    float4x4 projection;
};

[[vk::binding(0, 0)]]
ConstantBuffer<GlobalUniformData> ubo;

struct VertexInput
{
    [[vk::location(0)]]
    float2 inPosition;
    [[vk::location(1)]]
    float3 inColor;
};

struct VertexOutput
{
    float4 sv_position : SV_Position;
    [[vk::location(0)]]
    float3 fragColor;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;
    float4 pos = float4(input.inPosition, 0.0, 1.0);
    float4 worldPos = mul(ubo.model, pos);
    float4 viewPos  = mul(ubo.view, worldPos);
    output.sv_position = mul(ubo.projection, viewPos);
    output.fragColor = input.inColor;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    return float4(input.fragColor, 1.0);
}
