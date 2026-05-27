struct OverlayAaUniformData
{
    float4 screenParams; // xy = 1/size, zw unused
};

[[vk::binding(0, 0)]]
ConstantBuffer<OverlayAaUniformData> ubo;

[[vk::binding(1, 0)]]
Texture2D<float4> sceneColorTexture;

[[vk::binding(2, 0)]]
SamplerState sceneColorSampler;

[[vk::binding(3, 0)]]
Texture2D<float4> overlayColorTexture;

[[vk::binding(4, 0)]]
SamplerState overlayColorSampler;

[[vk::binding(5, 0)]]
Texture2D<float4> lineDataTexture;

[[vk::binding(6, 0)]]
SamplerState lineDataSampler;

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
    output.uv = uv * 0.5;
    return output;
}

float4 sampleOverlayAA(float2 uv)
{
    return overlayColorTexture.Sample(overlayColorSampler, uv);
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    float2 uv = input.uv;
    float4 scene = sceneColorTexture.Sample(sceneColorSampler, uv);
    float4 overlay = sampleOverlayAA(uv);
    float3 rgb = lerp(scene.rgb, overlay.rgb, overlay.a);
    return float4(rgb, scene.a);
}
