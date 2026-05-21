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
    float3 lightDir = normalize(ubo.lightDirection.xyz);
    float3 viewDir = normalize(ubo.cameraPosition.xyz - input.worldPos);
    float3 halfDir = normalize(lightDir + viewDir);

    float ndotl = saturate(dot(normal, lightDir));
    float ndoth = saturate(dot(normal, halfDir));

    float shadow = sampleShadowPCF(input.worldPos);

    float3 ambient = ubo.ambientColor.rgb * albedo;
    float3 diffuse =
        ubo.diffuseColor.rgb * albedo * ndotl * ubo.lightColor.rgb * shadow;
    float shininess = ubo.specularColorAndShininess.w;
    float3 specular = ubo.specularColorAndShininess.rgb * ubo.lightColor.rgb *
                      pow(ndoth, shininess) * shadow;

    float3 litColor = ambient + diffuse + specular;
    return float4(litColor, ubo.baseColorFactor.a);
}
