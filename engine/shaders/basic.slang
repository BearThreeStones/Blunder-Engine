struct VertexInput {
  [[vk::location(0)]] float2 inPosition;
  [[vk::location(1)]] float3 inColor;
};

// UBO (统一缓冲对象)
cbuffer GlobalUniforms : register(b0, space0) {
  float4x4 model;
  float4x4 view;
  float4x4 proj;
}

struct VertexOutput {
  float4 sv_position : SV_Position;
  [[vk::location(0)]] float3 fragColor;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
  VertexOutput output;
  float4 world_position = mul(model, float4(input.inPosition, 0.0, 1.0));
  float4 view_position = mul(view, world_position);
  output.sv_position = mul(proj, view_position);
  output.fragColor = input.inColor;
  return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
  return float4(input.fragColor, 1.0);
}
