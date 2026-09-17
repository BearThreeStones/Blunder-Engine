#include "volumetric_fog_common.slang"

// Cache key is this file only — bump when heightDensity in common.slang changes.

[[vk::binding(0, 0)]]
ConstantBuffer<VolumetricFogUniforms> ubo;

[[vk::binding(1, 0)]]
RWTexture3D<float4> densityVolume;

[shader("compute")]
[numthreads(4, 4, 4)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint3 grid = uint3(ubo.gridSize.xyz);
    if (id.x >= grid.x || id.y >= grid.y || id.z >= grid.z)
    {
        return;
    }

    float3 worldPos = froxelWorldPos(id, ubo.gridSize.xyz, ubo.jitterTemporal.xy,
                                     ubo.gridZParams.xyz, ubo.invViewProj,
                                     ubo.cameraPosFar.xyz);
    float rho = heightDensity(ubo.densityParams.x, ubo.densityParams.y, worldPos.z,
                              ubo.densityParams.z);
    float3 scattering = ubo.fogAlbedoG.rgb * rho;
    float extinction = max(dot(ubo.fogAlbedoG.rgb, float3(0.3333, 0.3333, 0.3334)) * rho, rho);
    densityVolume[id] = float4(scattering, extinction);
}
