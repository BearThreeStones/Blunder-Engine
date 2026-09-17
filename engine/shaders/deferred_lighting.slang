// Deferred Render Path lighting: fullscreen triangle that reads the G-buffer
// plus the shared offscreen depth and writes the viewport offscreen color.
//
// Directional and area lights stay on the UBO list (at most 32). Point and
// spot lights come from the Viewport froxel grid (shared mapping with
// froxel_fill.slang). Per-receiver UBO masks keep the existing 32-bit layout;
// clustered linking uses 256-bit masks without the forward 8-cap. No Bindless.

#include "display_transform.slang"
#include "froxel_common.slang"
#include "shadow_sampling_ubo.slang"

struct GpuSceneLight
{
    float4 positionType;
    float4 colorFlags;
    float4 emitRange;
    float4 coneArea;
    float4 axisX;
};

static const uint kMaxDeferredLights = 32;
static const uint kMaxReceiverSlots = 16384;
static const uint kReceiverSlotMask = 0x3FFFu;
static const uint kReceiverUnlitBit = 0x4000u;
static const uint kReceiverTwoSidedBit = 0x8000u;
static const uint kReceiverNoGeometry = 0xFFFFu;
static const uint kClusteredMaskWords = 8;

struct DeferredLightingUniformData
{
    float4x4 invViewProjection;
    float4x4 lightViewProjection;
    float4 cameraPosition;
    float4 shadowParams;    // x bias, y enabled, z inv shadow map size
    float4 ambientColor;
    float4 backgroundColor;
    float4 lightCount;      // x = uploaded directional/area list size
    float4x4 view;
    float4 froxelScreen;    // width, height, tilesX, tilesY
    float4 froxelZ;         // near, far, slices, tileSize
    float4 clusteredParams; // x = clustered light count, y = heatmap
    GpuSceneLight lights[kMaxDeferredLights];
    ShadowSamplingUniform shadowSampling;
};

[[vk::binding(0, 0)]]
ConstantBuffer<DeferredLightingUniformData> ubo;

[[vk::binding(1, 0)]]
Texture2D<float4> gbufferAlbedoAo;

[[vk::binding(2, 0)]]
Texture2D<float4> gbufferNormalMetalRough;

[[vk::binding(3, 0)]]
Texture2D<uint> gbufferReceiver;

[[vk::binding(4, 0)]]
Texture2D<float> depthTexture;

[[vk::binding(5, 0)]]
Texture2D<float> shadowMap;

[[vk::binding(6, 0)]]
SamplerComparisonState shadowSampler;

[[vk::binding(7, 0)]]
StructuredBuffer<uint> receiverLightMasks;

[[vk::binding(8, 0)]]
StructuredBuffer<GpuSceneLight> clusteredLights;

[[vk::binding(9, 0)]]
StructuredBuffer<uint> froxelLightIndices;

[[vk::binding(10, 0)]]
StructuredBuffer<uint> froxelLightCount;

[[vk::binding(11, 0)]]
StructuredBuffer<uint> clusteredReceiverMasks;

[[vk::binding(12, 0)]]
Texture2DArray<float> vsmPages;

[[vk::binding(13, 0)]]
StructuredBuffer<uint> vsmPageTable;

[[vk::binding(14, 0)]]
TextureCubeArray<float> pointCubes;

[[vk::binding(15, 0)]]
Texture2DArray<float> spotMaps;

#include "shadow_sampling.slang"

struct VertexOutput
{
    float4 sv_position : SV_Position;
};

[shader("vertex")]
VertexOutput vertexMain(uint vertexId : SV_VertexID)
{
    VertexOutput output;
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    output.sv_position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    return output;
}

float3 decodeOctNormal(float2 e)
{
    float2 f = e * 2.0 - 1.0;
    float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = saturate(-n.z);
    n.xy += float2(n.x >= 0.0 ? -t : t, n.y >= 0.0 ? -t : t);
    return normalize(n);
}

float3 reconstructWorldPos(float2 uv, float depth)
{
    float4 clip = float4(uv * 2.0 - 1.0, depth, 1.0);
    float4 world = mul(ubo.invViewProjection, clip);
    return world.xyz / world.w;
}

float sampleShadowPCF(float3 worldPos)
{
    if (ubo.shadowParams.y < 0.5 || ubo.shadowSampling.vsmParams.w > 0.5)
    {
        return 1.0;
    }

    float4 lightClip = mul(ubo.lightViewProjection, float4(worldPos, 1.0));
    float3 ndc = lightClip.xyz / lightClip.w;
    float2 uv = ndc.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;
    float refDepth = ndc.z - ubo.shadowParams.x;

    float texel = ubo.shadowParams.z;
    float2 offsets[4] = {
        float2(-texel, -texel),
        float2(texel, -texel),
        float2(-texel, texel),
        float2(texel, texel),
    };

    float shadow = 0.0;
    for (int i = 0; i < 4; ++i)
    {
        shadow += shadowMap.SampleCmp(shadowSampler, uv + offsets[i], refDepth);
    }
    return shadow * 0.25;
}

float distributionGGX(float ndoth, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = ndoth * ndoth * (a2 - 1.0) + 1.0;
    return a2 / max(3.14159265 * denom * denom, 0.0001);
}

float geometrySchlickGGX(float ndotv, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return ndotv / (ndotv * (1.0 - k) + k);
}

float geometrySmith(float ndotv, float ndotl, float roughness)
{
    return geometrySchlickGGX(ndotv, roughness) * geometrySchlickGGX(ndotl, roughness);
}

float3 fresnelSchlick(float cosTheta, float3 f0)
{
    return f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
}

float punctualAtten(float dist, float range)
{
    if (range <= 1e-6 || dist >= range)
    {
        return 0.0;
    }
    float invSq = 1.0 / max(dist * dist, 1e-8);
    float t = 1.0 - dist / range;
    return invSq * t * t;
}

float3 lightDirAndAtten(GpuSceneLight light, float3 worldPos, out float atten)
{
    float type = light.positionType.w;
    float3 emit = light.emitRange.xyz;
    float emitLen = length(emit);
    emit = emitLen > 1e-6 ? emit / emitLen : float3(0.0, 0.0, -1.0);
    atten = 1.0;
    if (type < 0.5)
    {
        return normalize(-emit);
    }
    float3 toLight = light.positionType.xyz - worldPos;
    float dist = length(toLight);
    float3 L = dist > 1e-6 ? toLight / dist : float3(0.0, 0.0, 1.0);
    atten = punctualAtten(dist, light.emitRange.w);
    if (type > 1.5 && type < 2.5)
    {
        float cd = dot(-L, emit);
        float innerCos = light.coneArea.x;
        float outerCos = light.coneArea.y;
        atten *= saturate((cd - outerCos) / max(innerCos - outerCos, 1e-5));
    }
    return L;
}

float3 shadePbrLight(float3 L, float3 radiance, float3 normal, float3 viewDir,
                     float3 albedo, float metallic, float roughness, float ao,
                     bool twoSided)
{
    // Two-sided materials shade back faces with the mirrored normal (abs), the
    // same rule pbr.slang applies from pbrTextureFlags.w.
    float3 halfDir = normalize(L + viewDir);
    float ndotl = twoSided ? abs(dot(normal, L)) : saturate(dot(normal, L));
    float ndotv = twoSided ? abs(dot(normal, viewDir)) : saturate(dot(normal, viewDir));
    float ndoth = twoSided ? abs(dot(normal, halfDir)) : saturate(dot(normal, halfDir));
    float hdotv = saturate(dot(halfDir, viewDir));
    float3 f0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float d = distributionGGX(ndoth, roughness);
    float g = geometrySmith(ndotv, ndotl, roughness);
    float3 f = fresnelSchlick(hdotv, f0);
    float3 specular = (d * g * f) / max(4.0 * ndotv * ndotl, 0.001);
    float3 diffuse = (1.0 - f) * (1.0 - metallic) * albedo / 3.14159265;
    return (diffuse + specular) * radiance * ndotl * ao;
}

float3 shadeAreaLight(GpuSceneLight light, float3 worldPos, float3 normal, float3 viewDir,
                      float3 albedo, float metallic, float roughness, float ao,
                      float shadowTerm, bool twoSided)
{
    float3 emit = light.emitRange.xyz;
    float emitLen = length(emit);
    emit = emitLen > 1e-6 ? emit / emitLen : float3(0.0, 0.0, -1.0);
    float3 center = light.positionType.xyz;
    if (dot(worldPos - center, emit) <= 0.0)
    {
        return float3(0.0, 0.0, 0.0);
    }
    float3 axisX = light.axisX.xyz;
    float axisLen = length(axisX);
    axisX = axisLen > 1e-6 ? axisX / axisLen : float3(1.0, 0.0, 0.0);
    float3 axisY = normalize(cross(emit, axisX));
    float3 accum = float3(0.0, 0.0, 0.0);
    float offsets[2] = { -0.25, 0.25 };
    for (int ix = 0; ix < 2; ++ix)
    {
        for (int iy = 0; iy < 2; ++iy)
        {
            float3 samplePos = center + axisX * (light.coneArea.z * offsets[ix]) +
                               axisY * (light.coneArea.w * offsets[iy]);
            float3 toLight = samplePos - worldPos;
            float dist = length(toLight);
            float3 L = dist > 1e-6 ? toLight / dist : float3(0.0, 0.0, 1.0);
            float atten = 1.0 / max(dist * dist, 1e-4);
            float3 radiance = light.colorFlags.rgb * atten * shadowTerm;
            accum += shadePbrLight(L, radiance, normal, viewDir, albedo, metallic,
                                   roughness, ao, twoSided);
        }
    }
    return accum * 0.25;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    int3 pixel = int3(int2(input.sv_position.xy), 0);
    uint tilesX = max(uint(ubo.froxelScreen.z), 1u);
    uint tilesY = max(uint(ubo.froxelScreen.w), 1u);
    uint slices = max(uint(ubo.froxelZ.z), 1u);
    uint tileSize = max(uint(ubo.froxelZ.w), 1u);
    uint tileX = min(uint(input.sv_position.x) / tileSize, tilesX - 1u);
    uint tileY = min(uint(input.sv_position.y) / tileSize, tilesY - 1u);
    if (ubo.clusteredParams.y > 0.5)
    {
        // Debug overlay: color the 64px screen tile by the busiest Z slice
        // so occupancy is visible even when the surface sits in an empty slice.
        uint listed = 0u;
        for (uint s = 0u; s < slices; ++s)
        {
            uint cell = froxelIndex(tileX, tileY, s, tilesX, tilesY);
            listed = max(listed, min(froxelLightCount[cell], kFroxelLightsPerCell));
        }
        float3 heat = occupancyHeatColor(listed);
        uint px = uint(input.sv_position.x) % tileSize;
        uint py = uint(input.sv_position.y) % tileSize;
        if (px == 0u || py == 0u)
        {
            heat *= 0.45;
        }
        return float4(heat, 1.0);
    }

    uint receiver = gbufferReceiver.Load(pixel);
    float depth = depthTexture.Load(pixel);
    // Receiver 0xFFFF is the G-buffer clear. Do not also discard on depth>=1:
    // centimetre Sponza at LOOKAT ~9000 with near 0.1 / far 100000 stores
    // ~0.99999, which lighting/SSAO used to treat as empty sky while Unique
    // gizmos (no depth test) stayed on the courtyard.
    if (receiver == kReceiverNoGeometry)
    {
        return float4(ubo.backgroundColor.rgb, 1.0);
    }

    float4 albedoAo = gbufferAlbedoAo.Load(pixel);
    float3 albedo = albedoAo.rgb;
    float ao = albedoAo.a;
    if ((receiver & kReceiverUnlitBit) != 0)
    {
        return float4(albedo, 1.0);
    }

    float4 nmr = gbufferNormalMetalRough.Load(pixel);
    float3 normal = decodeOctNormal(nmr.xy);
    float metallic = nmr.z;
    float roughness = nmr.w;
    bool twoSided = (receiver & kReceiverTwoSidedBit) != 0;

    uint depthWidth;
    uint depthHeight;
    depthTexture.GetDimensions(depthWidth, depthHeight);
    float2 uv = input.sv_position.xy / float2(depthWidth, depthHeight);
    float3 worldPos = reconstructWorldPos(uv, depth);
    float3 viewDir = normalize(ubo.cameraPosition.xyz - worldPos);
    float shadow = sampleShadowPCF(worldPos);

    uint slot = receiver & kReceiverSlotMask;
    uint mask = receiverLightMasks[slot];

    float3 color = float3(0.0, 0.0, 0.0);
    float occluder = 1.0;
    int count = int(ubo.lightCount.x);
    for (int i = 0; i < int(kMaxDeferredLights); ++i)
    {
        if (i >= count)
        {
            break;
        }
        if ((mask & (1u << uint(i))) == 0)
        {
            continue;
        }
        GpuSceneLight light = ubo.lights[i];
        // Point (1) and spot (2) are shaded from the froxel list, not the UBO.
        if (light.positionType.w > 0.5 && light.positionType.w < 2.5)
        {
            continue;
        }
        float shadowTerm = sampleLightShadowTerm(
            light, worldPos, shadow, shadowMap, shadowSampler,
            ubo.lightViewProjection, ubo.shadowParams, ubo.shadowSampling.vsmLightView,
            ubo.shadowSampling.vsmOriginLevels, ubo.shadowSampling.vsmParams,
            vsmPages, vsmPageTable, pointCubes, spotMaps,
            ubo.shadowSampling.spotViewProjections);
        if (light.colorFlags.w < 0.5)
        {
            occluder *= shadowTerm;
            continue;
        }
        if (light.positionType.w > 2.5)
        {
            color += shadeAreaLight(light, worldPos, normal, viewDir, albedo,
                                    metallic, roughness, ao, shadowTerm, twoSided);
            continue;
        }
        float atten = 1.0;
        float3 lightDir = lightDirAndAtten(light, worldPos, atten);
        float3 radiance = light.colorFlags.rgb * atten * shadowTerm;
        color += shadePbrLight(lightDir, radiance, normal, viewDir, albedo, metallic,
                               roughness, ao, twoSided);
    }

    float3 viewPos = mul(ubo.view, float4(worldPos, 1.0)).xyz;
    float viewZ = max(-viewPos.z, ubo.froxelZ.x);
    uint slice = viewZToSlice(viewZ, ubo.froxelZ.x, ubo.froxelZ.y, slices);
    uint cell = froxelIndex(tileX, tileY, slice, tilesX, tilesY);
    uint listed = min(froxelLightCount[cell], kFroxelLightsPerCell);
    for (uint i = 0; i < kFroxelLightsPerCell; ++i)
    {
        if (i >= listed)
        {
            break;
        }
        uint li = froxelLightIndices[cell * kFroxelLightsPerCell + i];
        if (li >= kMaxClusteredLights)
        {
            continue;
        }
        uint word = clusteredReceiverMasks[slot * kClusteredMaskWords + (li >> 5u)];
        if ((word & (1u << (li & 31u))) == 0)
        {
            continue;
        }
        GpuSceneLight light = clusteredLights[li];
        float shadowTerm = sampleLightShadowTerm(
            light, worldPos, shadow, shadowMap, shadowSampler,
            ubo.lightViewProjection, ubo.shadowParams, ubo.shadowSampling.vsmLightView,
            ubo.shadowSampling.vsmOriginLevels, ubo.shadowSampling.vsmParams,
            vsmPages, vsmPageTable, pointCubes, spotMaps,
            ubo.shadowSampling.spotViewProjections);
        if (light.colorFlags.w < 0.5)
        {
            occluder *= shadowTerm;
            continue;
        }
        float atten = 1.0;
        float3 lightDir = lightDirAndAtten(light, worldPos, atten);
        float3 radiance = light.colorFlags.rgb * atten * shadowTerm;
        color += shadePbrLight(lightDir, radiance, normal, viewDir, albedo, metallic,
                               roughness, ao, twoSided);
    }

    color += albedo * ubo.ambientColor.rgb * ao;
    color *= occluder;
    return float4(linearToSrgb(tonemapReinhard(color)), 1.0);
}
