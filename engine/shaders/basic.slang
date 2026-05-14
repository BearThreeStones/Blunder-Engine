struct GlobalUniformData
{
    float4x4 model;
    float4x4 view;
    float4x4 projection;
};

[[vk::binding(0, 0)]]
ConstantBuffer<GlobalUniformData> ubo;

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
    float3 worldNormal;
    [[vk::location(1)]]
    float2 uv;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;
    float4 pos = float4(input.inPosition, 1.0);
    float4 worldPos = mul(ubo.model, pos);
    float4 viewPos  = mul(ubo.view, worldPos);
    output.sv_position = mul(ubo.projection, viewPos);
    output.worldNormal = normalize(mul((float3x3)ubo.model, input.inNormal));
    output.uv = input.inUv;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    float3 normal = normalize(input.worldNormal);
    float3 lightDir = normalize(float3(-0.45, -0.7, -0.55));
    float lambert = saturate(dot(normal, -lightDir));
    float3 baseColor = baseColorTexture.Sample(baseColorSampler, input.uv).rgb;
    float3 litColor = baseColor * (0.2 + lambert * 0.8);
    return float4(litColor, 1.0);
}
