// Overlay anti-aliasing post-process shader.
// TODO: Implement Blender-style line AA compositing.
//
// This shader will read:
//   - depth_tx: scene depth for edge-aware blending
//   - color_tx: overlay color rendered by all overlay passes
//   - line_tx:  per-pixel line direction + signed distance (MRT output)
//
// And output a smooth, anti-aliased composite of all overlay lines.
//
// References:
//   Blender: source/blender/draw/engines/overlay/shaders/overlay_antialiasing_frag.glsl
//   line_tx layout: RG = line direction (normalized), BA = signed distance + thickness

struct VSOutput {
  float4 position : SV_Position;
  float2 uv       : TEXCOORD0;
};

// Fullscreen triangle: 3 vertices procedurally generated.
[shader("vertex")]
VSOutput vertexMain(uint vertexId : SV_VertexID) {
  VSOutput output;
  // Generate fullscreen triangle covering [-1,1] clip space.
  float2 pos = float2((vertexId << 1) & 2, vertexId & 2);
  output.position = float4(pos * 2.0 - 1.0, 0.0, 1.0);
  output.uv = float2(pos.x, 1.0 - pos.y);
  return output;
}

// TODO: bind color_tx, depth_tx, line_tx as Texture2D uniforms
// TODO: implement AA blending logic

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target {
  // Passthrough placeholder — just output transparent.
  return float4(0.0, 0.0, 0.0, 0.0);
}
