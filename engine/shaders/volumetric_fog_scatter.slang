#include "volumetric_fog_common.slang"

// Local inject uses windowed range atten (no 1/r^2) so point/spot origins
// do not bloom into UNORM orbs. Keep in lockstep with volumetricFogLocalAtten.

[[vk::binding(0, 0)]]
ConstantBuffer<VolumetricFogUniforms> ubo;

[[vk::binding(1, 0)]]
Texture3D<float4> densityVolume;

[[vk::binding(2, 0)]]
SamplerState densitySampler;

[[vk::binding(3, 0)]]
Texture3D<float4> historyVolume;

[[vk::binding(4, 0)]]
SamplerState historySampler;

[[vk::binding(5, 0)]]
RWTexture3D<float4> scatterVolume;

[shader("compute")]
[numthreads(4, 4, 4)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint3 grid = uint3(ubo.gridSize.xyz);
    if (id.x >= grid.x || id.y >= grid.y || id.z >= grid.z)
    {
        return;
    }

    float3 uvw = (float3(id) + 0.5) / ubo.gridSize.xyz;
    float4 density = densityVolume.SampleLevel(densitySampler, uvw, 0);
    float3 scattering = density.rgb;
    float extinction = density.a;

    float3 worldPos = froxelWorldPos(id, ubo.gridSize.xyz, ubo.jitterTemporal.xy,
                                     ubo.gridZParams.xyz, ubo.invViewProj,
                                     ubo.cameraPosFar.xyz);
    float3 viewDir = normalize(ubo.cameraPosFar.xyz - worldPos);
    float g = ubo.fogAlbedoG.w;
    float shadowFactor = 1.0;
    float3 inScatter = 0.0.xxx;
    if (ubo.lightColor.w > 0.5)
    {
        float3 lightL = -normalize(ubo.lightDir.xyz);
        float phase = henyeyGreenstein(dot(viewDir, lightL), g);
        inScatter = ubo.lightColor.rgb * scattering * phase * shadowFactor;
    }

    uint localCount = min(uint(ubo.localLightCount.x), kMaxVolumetricFogLocalLights);
    for (uint i = 0; i < kMaxVolumetricFogLocalLights; ++i)
    {
        if (i >= localCount)
        {
            break;
        }
        FogLocalLight light = ubo.localLights[i];
        if (light.colorFlags.w < 0.5)
        {
            continue;
        }
        float atten = 1.0;
        float3 L = fogLocalLightDirAndAtten(light, worldPos, ubo.densityParams.w, atten);
        if (atten <= 0.0)
        {
            continue;
        }
        float phase = henyeyGreenstein(dot(viewDir, L), g);
        inScatter += light.colorFlags.rgb * atten * scattering * phase * shadowFactor;
    }

    float3 current = inScatter;
    bool historyValid = ubo.jitterTemporal.w > 0.5;
    if (historyValid)
    {
        float4 prevClip = mul(ubo.prevViewProj, float4(worldPos, 1.0));
        float3 prevNdc = prevClip.xyz / max(prevClip.w, 1e-6);
        float3 prevUvw = float3(prevNdc.xy * 0.5 + 0.5,
                                sliceFromViewZ(length(worldPos - ubo.cameraPosFar.xyz),
                                               ubo.gridZParams.xyz) / ubo.gridSize.z);
        bool inFrustum = prevUvw.x >= 0.0 && prevUvw.x <= 1.0 &&
                         prevUvw.y >= 0.0 && prevUvw.y <= 1.0 &&
                         prevUvw.z >= 0.0 && prevUvw.z <= 1.0;
        if (inFrustum)
        {
            float3 history = historyVolume.SampleLevel(historySampler, prevUvw, 0).rgb;
            current = lerp(history, current, ubo.jitterTemporal.z);
        }
    }

    scatterVolume[id] = float4(current, extinction);
}
