struct GpuSceneLight
{
    float4 positionType;
    float4 colorFlags;
    float4 emitRange;
    float4 coneArea;
    float4 axisX;
};

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
};

[[vk::binding(0, 0)]]
ConstantBuffer<GlobalUniformData> ubo;

[[vk::binding(1, 0)]]
Texture2D<float> shadowMap;

[[vk::binding(2, 0)]]
SamplerComparisonState shadowSampler;

static const uint kMaxSkinJoints = 128;

struct BonePaletteData
{
    float4x4 jointMatrices[kMaxSkinJoints];
};

[[vk::binding(3, 0)]]
ConstantBuffer<BonePaletteData> bonePalette;

[[vk::binding(0, 1)]]
Texture2D<float4> bindlessTextures[];

[[vk::binding(1, 1)]]
SamplerState bindlessSamplers[];

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
    int4 inJointIndices;
    [[vk::location(5)]]
    float4 inWeights;
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
};

float4 sampleBindless(uint index, float2 uv)
{
    uint i = NonUniformResourceIndex(index);
    return bindlessTextures[i].Sample(bindlessSamplers[i], uv);
}

float4x4 blendSkinMatrix(float4 weights, int4 joints)
{
    float4x4 result = (float4x4)0;
    float weight_sum = weights.x + weights.y + weights.z + weights.w;
    if (weight_sum <= 1e-6)
    {
        return float4x4(
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1);
    }

    if (weights.x > 0.0 && joints.x >= 0 && joints.x < (int)kMaxSkinJoints)
    {
        result += bonePalette.jointMatrices[joints.x] * weights.x;
    }
    if (weights.y > 0.0 && joints.y >= 0 && joints.y < (int)kMaxSkinJoints)
    {
        result += bonePalette.jointMatrices[joints.y] * weights.y;
    }
    if (weights.z > 0.0 && joints.z >= 0 && joints.z < (int)kMaxSkinJoints)
    {
        result += bonePalette.jointMatrices[joints.z] * weights.z;
    }
    if (weights.w > 0.0 && joints.w >= 0 && joints.w < (int)kMaxSkinJoints)
    {
        result += bonePalette.jointMatrices[joints.w] * weights.w;
    }

    if (weight_sum < 0.999 || weight_sum > 1.001)
    {
        result /= weight_sum;
    }
    return result;
}

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;
    float4x4 skinMatrix = blendSkinMatrix(input.inWeights, input.inJointIndices);
    float4 skinnedPos = mul(skinMatrix, float4(input.inPosition, 1.0));
    float3 skinnedNormal = mul((float3x3)skinMatrix, input.inNormal);

    float4 worldPos = mul(ubo.model, skinnedPos);
    float4 viewPos = mul(ubo.view, worldPos);
    output.sv_position = mul(ubo.projection, viewPos);
    output.worldNormal =
        normalize(mul((float3x3)ubo.normalMatrix, skinnedNormal));
    output.worldTangent =
        normalize(mul((float3x3)ubo.normalMatrix, input.inTangent.xyz));
    output.tangentSign = input.inTangent.w;
    output.worldPos = worldPos.xyz;
    output.uv = input.inUv;
    return output;
}

float sampleShadowPCF(float3 worldPos)
{
    if (ubo.shadowParams.y < 0.5)
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

float3 getNormalFromMap(VertexOutput input)
{
    if (ubo.pbrTextureFlags.y < 0.5)
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
    float3 mapNormal = sampleBindless(ubo.bindlessTextureIndices.z, input.uv).xyz * 2.0 - 1.0;
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

float3 shadePbrLight(float3 L, float3 radiance, float3 normal, float3 viewDir,
                     float3 albedo, float metallic, float roughness, float ao)
{
    // The mesh pipeline rasterizes both sides, so a two-sided material must
    // shade its back faces with the mirrored normal. abs() does that without
    // depending on the camera, unlike flipping the normal to face the viewer.
    bool twoSided = ubo.pbrTextureFlags.w > 0.5;
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
                      float3 albedo, float metallic, float roughness, float ao, float shadowTerm)
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
            accum += shadePbrLight(L, radiance, normal, viewDir, albedo, metallic, roughness, ao);
        }
    }
    return accum * 0.25;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    float4 baseSample = sampleBindless(ubo.bindlessTextureIndices.x, input.uv);
    float3 albedo = baseSample.rgb * ubo.baseColorFactor.rgb;
    float alpha = baseSample.a * ubo.baseColorFactor.a;

    if (ubo.materialFlags.x > 0.5)
    {
        return float4(albedo, alpha);
    }

    if (ubo.metallicRoughnessFactors.w > 1.5)
    {
        clip(alpha - ubo.metallicRoughnessFactors.z);
    }

    float metallic = ubo.metallicRoughnessFactors.x;
    float roughness = ubo.metallicRoughnessFactors.y;
    if (ubo.pbrTextureFlags.x > 0.5)
    {
        float4 mr = sampleBindless(ubo.bindlessTextureIndices.y, input.uv);
        roughness *= mr.g;
        metallic *= mr.b;
    }

    float ao = 1.0;
    if (ubo.pbrTextureFlags.z > 0.5)
    {
        ao = sampleBindless(ubo.bindlessTextureIndices.w, input.uv).r;
    }

    float3 normal = getNormalFromMap(input);
    float3 viewDir = normalize(ubo.cameraPosition.xyz - input.worldPos);
    float shadow = sampleShadowPCF(input.worldPos);

    float3 color = float3(0.0, 0.0, 0.0);
    int count = int(ubo.lightCount.x);
    for (int i = 0; i < 8; ++i)
    {
        if (i >= count)
        {
            break;
        }
        GpuSceneLight light = ubo.lights[i];
        if (light.colorFlags.w < 0.5)
        {
            continue;
        }
        float shadowTerm = light.axisX.w > 0.5 ? shadow : 1.0;
        if (light.positionType.w > 2.5)
        {
            color += shadeAreaLight(light, input.worldPos, normal, viewDir, albedo,
                                    metallic, roughness, ao, shadowTerm);
            continue;
        }
        float atten = 1.0;
        float3 lightDir = lightDirAndAtten(light, input.worldPos, atten);
        float3 radiance = light.colorFlags.rgb * atten * shadowTerm;
        color += shadePbrLight(lightDir, radiance, normal, viewDir, albedo, metallic,
                               roughness, ao);
    }
    color += albedo * ubo.ambientColor.rgb * ao;
    return float4(color, alpha);
}
