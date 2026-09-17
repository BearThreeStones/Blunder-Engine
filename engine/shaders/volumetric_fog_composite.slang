#include "display_transform.slang"
#include "volumetric_fog_common.slang"

[[vk::binding(0, 0)]]
ConstantBuffer<VolumetricFogUniforms> ubo;

[[vk::binding(1, 0)]]
Texture2D<float4> sceneColorTexture;

[[vk::binding(2, 0)]]
SamplerState sceneColorSampler;

[[vk::binding(3, 0)]]
Texture2D<float> depthTexture;

[[vk::binding(4, 0)]]
SamplerState depthSampler;

[[vk::binding(5, 0)]]
Texture3D<float4> integratedVolume;

[[vk::binding(6, 0)]]
SamplerState integratedSampler;

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
    // Covering-triangle verts sit at NDC -1 and +3 with uv 0/2 so the visible
    // NDC [-1,1] square interpolates to [0,1]. uv*0.5 maps the screen to
    // [0,0.5] and samples only the top-left quarter — LOOKAT Sponza then
    // shows empty sky while Unique gizmos (drawn after) stay on the courtyard.
    output.uv = uv;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    float2 uv = input.uv;
    float4 scene = sceneColorTexture.Sample(sceneColorSampler, uv);
    float depth = depthTexture.Sample(depthSampler, uv).r;

    float viewZ;
    // glm::perspectiveZO plus a 1.0 depth clear: 1 is far / empty sky, matching
    // deferred_lighting.slang. Unprojecting it with Viewport far 100000 gives a
    // point 100 km out, so take the volume far slice directly instead.
    if (depth >= 1.0)
    {
        viewZ = ubo.cameraPosFar.w;
    }
    else
    {
        float4 clip = float4(uv * 2.0 - 1.0, depth, 1.0);
        float4 worldH = mul(ubo.invViewProj, clip);
        float3 worldPos = worldH.xyz / max(worldH.w, 1e-6);
        viewZ = length(worldPos - ubo.cameraPosFar.xyz);
    }
    viewZ = clamp(viewZ, ubo.gridZParams.w, ubo.cameraPosFar.w);

    float slice = sliceFromViewZ(viewZ, ubo.gridZParams.xyz);
    float3 uvw = float3(uv, (slice + 0.5) / ubo.gridSize.z);
    float4 fogSample = integratedVolume.SampleLevel(integratedSampler, uvw, 0);
    float transmittance = fogSample.a;
    // Deferred lighting already encoded the presented colour, so decode before
    // blending: `scene * T + inscatter` in sRGB space darkens the fog wrongly.
    // Inscatter is tonemapped into the same 0..1 display range as the scene so
    // local peaks cannot bloom into white orbs.
    float3 sceneLinear = srgbToLinear(scene.rgb);
    float3 inscatter = tonemapReinhard(fogSample.rgb);
    return float4(linearToSrgb(sceneLinear * transmittance + inscatter), scene.a);
}
