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
    float4 lightCount;
    GpuSceneLight lights[8];
};

[[vk::binding(0, 0)]]
ConstantBuffer<GlobalUniformData> ubo;

[[vk::binding(1, 0)]]
Texture2D<float4> baseColorTexture;

[[vk::binding(2, 0)]]
SamplerState baseColorSampler;

[[vk::binding(3, 0)]]
Texture2D<float> shadowMap;

[[vk::binding(4, 0)]]
SamplerComparisonState shadowSampler;

struct VertexInput
{
    [[vk::location(0)]]
    float3 inPosition;
    [[vk::location(1)]]
    float3 inNormal;
    [[vk::location(2)]]
    float2 inUv;
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
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;
    float4 pos = float4(input.inPosition, 1.0);
    float4 worldPos = mul(ubo.model, pos);
    float4 viewPos = mul(ubo.view, worldPos);
    output.sv_position = mul(ubo.projection, viewPos);
    output.worldNormal =
        normalize(mul((float3x3)ubo.normalMatrix, input.inNormal));
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

float3 evaluateAreaLight(GpuSceneLight light, float3 worldPos, float3 normal, float3 viewDir)
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
            float ndotl = saturate(dot(normal, L));
            float atten = 1.0 / max(dist * dist, 1e-4);
            float3 viewDirN = viewDir;
            float3 halfDir = normalize(L + viewDirN);
            float ndoth = saturate(dot(normal, halfDir));
            float3 diffuse = ubo.diffuseColor.rgb * ndotl;
            float shininess = ubo.specularColorAndShininess.w;
            float3 specular = ubo.specularColorAndShininess.rgb * pow(ndoth, shininess);
            accum += (diffuse + specular) * light.colorFlags.rgb * atten;
        }
    }
    return accum * 0.25;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    float3 albedo =
        baseColorTexture.Sample(baseColorSampler, input.uv).rgb * ubo.baseColorFactor.rgb;

    if (ubo.materialFlags.x > 0.5)
    {
        return float4(albedo, ubo.baseColorFactor.a);
    }

    float3 normal = normalize(input.worldNormal);
    float3 viewDir = normalize(ubo.cameraPosition.xyz - input.worldPos);
    float shadow = sampleShadowPCF(input.worldPos);

    float3 lit = float3(0.0, 0.0, 0.0);
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
            lit += evaluateAreaLight(light, input.worldPos, normal, viewDir) * albedo * shadowTerm;
            continue;
        }
        float atten = 1.0;
        float3 lightDir = lightDirAndAtten(light, input.worldPos, atten);
        float3 halfDir = normalize(lightDir + viewDir);
        float ndotl = saturate(dot(normal, lightDir));
        float ndoth = saturate(dot(normal, halfDir));
        float3 radiance = light.colorFlags.rgb * atten * shadowTerm;
        float3 diffuse = ubo.diffuseColor.rgb * albedo * ndotl * radiance;
        float shininess = ubo.specularColorAndShininess.w;
        float3 specular = ubo.specularColorAndShininess.rgb * radiance * pow(ndoth, shininess);
        lit += diffuse + specular;
    }

    float3 ambient = ubo.ambientColor.rgb * albedo;
    return float4(ambient + lit, ubo.baseColorFactor.a);
}
