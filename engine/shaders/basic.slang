// basic.slang - Basic vertex/fragment shader for Blunder Engine
//
// Uses Slang syntax with explicit entry point decorators.

struct VertexOutput
{
    float4 sv_position : SV_Position;
    float3 color : COLOR;
};

[shader("vertex")]
VertexOutput vertexMain(uint vertexIndex: SV_VertexID)
{
    float2 positions[3] = { float2(0.0, -0.5), float2(0.5, 0.5), float2(-0.5, 0.5) };
    float3 colors[3] = { float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), float3(0.0, 0.0, 1.0) };
    VertexOutput output;
    output.sv_position = float4(positions[vertexIndex], 0.0, 1.0);
    output.color = colors[vertexIndex];
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    return float4(input.color, 1.0);
}
