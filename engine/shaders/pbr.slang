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
};

[[vk::binding(0, 0)]]
ConstantBuffer<GlobalUniformData> ubo;

[[vk::binding(1, 0)]]
Texture2D<float4> baseColorTexture;

[[vk::binding(2, 0)]]
SamplerState baseColorSampler;

[[vk::binding(3, 0)]]
Texture2D<float> shadowMap;

[[vk::binding(3, 0)]]
SamplerComparisonState shadowSampler;

[[vk::binding(4, 0)]]
Texture2D<float4> metallicRoughnessTexture;

[[vk::binding(5, 0)]]
SamplerState metallicRoughnessSampler;

[[vk::binding(6, 0)]]
Texture2D<float4> normalTexture;

[[vk::binding(7, 0)]]
SamplerState normalSampler;

[[vk::binding(8, 0)]]
Texture2D<float4> occlusionTexture;

[[vk::binding(9, 0)]]
SamplerState occlusionSampler;

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

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;
    float4 worldPos = mul(ubo.model, float4(input.inPosition, 1.0));
    float4 viewPos = mul(ubo.view, worldPos);
    output.sv_position = mul(ubo.projection, viewPos);
    output.worldNormal =
        normalize(mul((float3x3)ubo.normalMatrix, input.inNormal));
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

    float3 tangent = normalize(input.worldTangent);
    float3 bitangent = normalize(cross(input.worldNormal, tangent)) * input.tangentSign;
    float3x3 tbn = float3x3(tangent, bitangent, normalize(input.worldNormal));
    float3 mapNormal = normalTexture.Sample(normalSampler, input.uv).xyz * 2.0 - 1.0;
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

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    float4 baseSample = baseColorTexture.Sample(baseColorSampler, input.uv);
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
        float4 mr = metallicRoughnessTexture.Sample(metallicRoughnessSampler, input.uv);
        roughness *= mr.g;
        metallic *= mr.b;
    }

    float ao = 1.0;
    if (ubo.pbrTextureFlags.z > 0.5)
    {
        ao = occlusionTexture.Sample(occlusionSampler, input.uv).r;
    }

    float3 normal = getNormalFromMap(input);
    float3 viewDir = normalize(ubo.cameraPosition.xyz - input.worldPos);
    float3 lightDir = normalize(ubo.lightDirection.xyz);
    float3 halfDir = normalize(lightDir + viewDir);

    float ndotl = saturate(dot(normal, lightDir));
    float ndotv = saturate(dot(normal, viewDir));
    float ndoth = saturate(dot(normal, halfDir));
    float hdotv = saturate(dot(halfDir, viewDir));

    float3 f0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float shadow = sampleShadowPCF(input.worldPos);

    float d = distributionGGX(ndoth, roughness);
    float g = geometrySmith(ndotv, ndotl, roughness);
    float3 specular = (d * g * fresnelSchlick(hdotv, f0)) / max(4.0 * ndotv * ndotl, 0.001);

    float3 diffuse = (1.0 - specular) * (1.0 - metallic) * albedo / 3.14159265;
    float3 radiance = ubo.lightColor.rgb * shadow;
  float3 color = (diffuse + specular) * radiance * ndotl * ao;
  color += albedo * ubo.ambientColor.rgb * ao;
    return float4(color, alpha);
}
