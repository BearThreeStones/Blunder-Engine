#include "gpu_driven_common.slang"
#include "vertex_color.slang"

struct GpuSceneLight
{
    float4 positionType;
    float4 colorFlags;
    float4 emitRange;
    float4 coneArea;
    float4 axisX;
};

#include "shadow_sampling_ubo.slang"

struct GlobalUniformData
{
    float4x4 model;
    float4x4 view;
    float4x4 projection;
    float4 cameraPosition;
    float4 lightDirection;
    float4 lightColor;
    float4 baseColorFactor;
    float4 ambientColor;
    float4 diffuseColor;
    float4 specularColorAndShininess;
    float4 materialFlags;
    float4x4 normalMatrix;
    float4x4 lightViewProjection;
    float4 shadowParams;
    float4 metallicRoughnessFactors;
    float4 pbrTextureFlags;
    uint4 bindlessTextureIndices;
    float4 lightCount;
    GpuSceneLight lights[8];
    ShadowSamplingUniform shadowSampling;
};

[[vk::binding(0, 0)]]
ConstantBuffer<GlobalUniformData> ubo;

[[vk::binding(1, 0)]]
Texture2D<float> shadowMap;

[[vk::binding(2, 0)]]
SamplerComparisonState shadowSampler;

[[vk::binding(3, 0)]]
StructuredBuffer<GpuDrivenInstanceGpu> instances;

[[vk::binding(8, 0)]]
StructuredBuffer<uint> compactIds;

[[vk::binding(4, 0)]]
Texture2DArray<float> vsmPages;

[[vk::binding(5, 0)]]
StructuredBuffer<uint> vsmPageTable;

[[vk::binding(6, 0)]]
TextureCubeArray<float> pointCubes;

[[vk::binding(7, 0)]]
Texture2DArray<float> spotMaps;

[[vk::binding(0, 1)]]
Texture2D<float4> bindlessTextures[];

[[vk::binding(1, 1)]]
SamplerState bindlessSamplers[];

#include "shadow_sampling.slang"

struct VertexInput
{
    [[vk::location(0)]]
    float3 inPosition;
    [[vk::location(1)]]
    float3 inNormal;
    [[vk::location(2)]]
    float2 inUv;
    [[vk::location(3)]]
    float4 inTangent;
    [[vk::location(4)]]
    float4 inColor;
};

struct VertexOutput
{
    float4 sv_position : SV_Position;
    [[vk::location(0)]]
    float3 worldNormal;
    [[vk::location(1)]]
    float2 uv;
    [[vk::location(2)]]
    float3 worldPos;
    [[vk::location(3)]]
    float3 worldTangent;
    [[vk::location(4)]]
    float tangentSign;
    [[vk::location(5)]]
    nointerpolation uint instanceId;
    [[vk::location(6)]]
    float4 color;
};

float4 sampleBindless(uint index, float2 uv)
{
    uint i = NonUniformResourceIndex(index);
    return bindlessTextures[i].Sample(bindlessSamplers[i], uv);
}

[shader("vertex")]
VertexOutput vertexMain(VertexInput input, uint instanceId : SV_VulkanInstanceID)
{
    uint instIndex = compactIds[instanceId];
    GpuDrivenInstanceGpu inst = instances[instIndex];
    VertexOutput output;
    float4 worldPos = mul(inst.world, float4(input.inPosition, 1.0));
    float4 viewPos = mul(ubo.view, worldPos);
    output.sv_position = mul(ubo.projection, viewPos);
    output.worldNormal =
        normalize(mul((float3x3)inst.normalMatrix, input.inNormal));
    output.worldTangent =
        normalize(mul((float3x3)inst.normalMatrix, input.inTangent.xyz));
    output.tangentSign = input.inTangent.w;
    output.worldPos = worldPos.xyz;
    output.uv = input.inUv;
    output.instanceId = instIndex;
    output.color = input.inColor;
    return output;
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

float3 getNormalFromMap(VertexOutput input, GpuDrivenInstanceGpu inst, float4 sampledNormal)
{
    if (inst.pbrTextureFlags.y < 0.5)
    {
        return normalize(input.worldNormal);
    }

    float3 tangent = input.worldTangent;
    if (dot(tangent, tangent) < 1e-4)
    {
        return normalize(input.worldNormal);
    }
    tangent = normalize(tangent);
    float3 bitangent = normalize(cross(input.worldNormal, tangent)) * input.tangentSign;
    float3x3 tbn = float3x3(tangent, bitangent, normalize(input.worldNormal));
    float3 mapNormal = sampledNormal.xyz * 2.0 - 1.0;
    return normalize(mul(tbn, mapNormal));
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

float3 shadePbrLight(float ndotlScale, float3 L, float3 radiance, float3 normal,
                     float3 viewDir, float3 albedo, float metallic, float roughness,
                     float ao, bool twoSided)
{
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
    return (diffuse + specular) * radiance * ndotl * ao * ndotlScale;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    GpuDrivenInstanceGpu inst = instances[input.instanceId];
    float4 sampledAlbedo = sampleBindless(inst.bindlessTextureIndices.x, input.uv);
    float4 sampledMr = sampleBindless(inst.bindlessTextureIndices.y, input.uv);
    float4 sampledNormal = sampleBindless(inst.bindlessTextureIndices.z, input.uv);
    float4 sampledAo = sampleBindless(inst.bindlessTextureIndices.w, input.uv);
    float4 baseSample = inst.materialFlags.y > 0.5 ? sampledAlbedo : float4(1.0, 1.0, 1.0, 1.0);
    float3 albedo = applyVertexColorToAlbedo(
        baseSample.rgb * inst.baseColorFactor.rgb, input.color.rgb,
        inst.materialFlags.y);
    float alpha = baseSample.a * inst.baseColorFactor.a * input.color.a;
    clipPackedVertexColorCoverage(input.color.rgb, inst.materialFlags.y);

    if (inst.materialFlags.x > 0.5)
    {
        return float4(albedo, alpha);
    }

    if (inst.metallicRoughnessFactors.w > 0.5 &&
        inst.metallicRoughnessFactors.w < 1.5)
    {
        clip(alpha - inst.metallicRoughnessFactors.z);
    }

    float metallic = inst.metallicRoughnessFactors.x;
    float roughness = inst.metallicRoughnessFactors.y;
    if (inst.pbrTextureFlags.x > 0.5)
    {
        roughness *= sampledMr.g;
        metallic *= sampledMr.b;
    }

    float ao = inst.pbrTextureFlags.z > 0.5 ? sampledAo.r : 1.0;
    float3 normal = getNormalFromMap(input, inst, sampledNormal);
    float3 viewDir = normalize(ubo.cameraPosition.xyz - input.worldPos);
    float shadow = sampleShadowPCF(input.worldPos);
    bool twoSided = inst.pbrTextureFlags.w > 0.5;

    float3 color = float3(0.0, 0.0, 0.0);
    float occluder = 1.0;
    int count = int(ubo.lightCount.x);
    for (int i = 0; i < 8; ++i)
    {
        if (i >= count)
        {
            break;
        }
        GpuSceneLight light = ubo.lights[i];
        if ((inst.lightMask & (1u << i)) == 0u)
        {
            continue;
        }
        float shadowTerm = sampleLightShadowTerm(
            light, input.worldPos, shadow, shadowMap, shadowSampler,
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
        float3 lightDir = lightDirAndAtten(light, input.worldPos, atten);
        float3 radiance = light.colorFlags.rgb * atten * shadowTerm;
        color += shadePbrLight(1.0, lightDir, radiance, normal, viewDir, albedo,
                               metallic, roughness, ao, twoSided);
    }
    color += albedo * ubo.ambientColor.rgb * ao;
    color *= occluder;
    return float4(color, alpha);
}
