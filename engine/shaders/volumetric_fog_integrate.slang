#include "volumetric_fog_common.slang"

[[vk::binding(0, 0)]]
ConstantBuffer<VolumetricFogUniforms> ubo;

[[vk::binding(1, 0)]]
Texture3D<float4> scatterVolume;

[[vk::binding(2, 0)]]
SamplerState scatterSampler;

[[vk::binding(3, 0)]]
RWTexture3D<float4> integratedVolume;

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint3 grid = uint3(ubo.gridSize.xyz);
    if (id.x >= grid.x || id.y >= grid.y)
    {
        return;
    }

    float3 accumulatedInscatter = 0.0.xxx;
    float transmittance = 1.0;

    for (uint z = 0; z < grid.z; ++z)
    {
        float3 uvw = (float3(id.xy, z) + 0.5) / ubo.gridSize.xyz;
        float4 scatter = scatterVolume.SampleLevel(scatterSampler, uvw, 0);
        float3 lighting = scatter.rgb;
        float extinction = max(scatter.a, 1e-6);
        float dt = froxelSliceDepth(z, ubo.gridZParams.xyz);
        float sliceT = exp(-extinction * dt);
        float3 sliceInscatter = (lighting - lighting * sliceT) / extinction;
        accumulatedInscatter += transmittance * sliceInscatter;
        transmittance *= sliceT;
        integratedVolume[uint3(id.xy, z)] = float4(accumulatedInscatter, transmittance);
    }
}
