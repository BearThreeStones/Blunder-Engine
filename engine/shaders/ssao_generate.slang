struct SsaOUniformData
{
    float4x4 invProjection;
    float4x4 projection;
    float4 clipParams; // x=near, y=far, z=radius, w=bias
    float4 screenParams; // xy=invResolution, zw=resolution
    float4 effectParams; // x=enabled, y=strength, z=power, w=unused
};

[[vk::binding(0, 0)]]
ConstantBuffer<SsaOUniformData> ubo;

[[vk::binding(1, 0)]]
Texture2D<float> depthTexture;

[[vk::binding(2, 0)]]
SamplerState depthSampler;

[[vk::binding(3, 0)]]
Texture2D<float4> noiseTexture;

[[vk::binding(4, 0)]]
SamplerState noiseSampler;

static const int kSampleCount = 12;
static const float3 kKernel[kSampleCount] = {
    float3(0.04977, 0.04471, 0.00496),
    float3(0.09648, 0.01414, 0.01980),
    float3(0.06924, 0.07435, 0.01344),
    float3(0.03459, 0.12158, 0.01888),
    float3(-0.03601, 0.10060, 0.01922),
    float3(-0.08927, 0.05856, 0.01117),
    float3(-0.10285, -0.02306, 0.01653),
    float3(-0.07598, -0.07276, 0.01026),
    float3(-0.03866, -0.10952, 0.01448),
    float3(0.03287, -0.10990, 0.01217),
    float3(0.08840, -0.07435, 0.00825),
    float3(0.12096, -0.01414, 0.00690),
};

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
    output.uv = uv * 0.5;
    return output;
}

float3 reconstructViewPos(float2 uv, float depth)
{
    float4 clip = float4(uv * 2.0 - 1.0, depth, 1.0);
    float4 view = mul(ubo.invProjection, clip);
    return view.xyz / view.w;
}

float computeRawAo(float2 uv, float3 origin)
{
    float2 noiseScale = ubo.screenParams.zw / 4.0;
    float3 randomVec = noiseTexture.Sample(noiseSampler, uv * noiseScale).xyz * 2.0 - 1.0;
    randomVec.z = 0.0;
    randomVec = normalize(randomVec);

    float3 tangent = normalize(randomVec - origin * dot(randomVec, origin));
    float3 bitangent = cross(origin, tangent);
    float3x3 tbn = float3x3(tangent, bitangent, origin);

    float occlusion = 0.0;
    float radius = ubo.clipParams.z;
    float bias = ubo.clipParams.w;

    for (int i = 0; i < kSampleCount; ++i)
    {
        float3 samplePos = mul(tbn, kKernel[i]) * radius + origin;
        float4 offset = mul(ubo.projection, float4(samplePos, 1.0));
        offset.xyz /= offset.w;
        float2 sampleUv = offset.xy * 0.5 + 0.5;
        sampleUv.y = 1.0 - sampleUv.y;

        float sampleDepth = depthTexture.Sample(depthSampler, sampleUv).r;
        float3 sampleView = reconstructViewPos(sampleUv, sampleDepth);
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(origin.z - sampleView.z));
        occlusion += (sampleView.z >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }

    return 1.0 - (occlusion / float(kSampleCount));
}

[shader("fragment")]
float fragmentMain(VertexOutput input) : SV_Target
{
    if (ubo.effectParams.x < 0.5)
    {
        return 1.0;
    }

    float2 uv = input.uv;
    float depth = depthTexture.Sample(depthSampler, uv).r;
    if (depth >= 0.9999)
    {
        return 1.0;
    }

    float3 origin = reconstructViewPos(uv, depth);
    float ao = computeRawAo(uv, origin);

    float2 texel = ubo.screenParams.xy;
    float2 offsets[4] = {
        float2(texel.x, 0.0),
        float2(-texel.x, 0.0),
        float2(0.0, texel.y),
        float2(0.0, -texel.y),
    };
    float blurred = ao;
    for (int j = 0; j < 4; ++j)
    {
        float2 sampleUv = uv + offsets[j];
        float sampleDepth = depthTexture.Sample(depthSampler, sampleUv).r;
        if (sampleDepth >= 0.9999)
        {
            continue;
        }
        float3 sampleOrigin = reconstructViewPos(sampleUv, sampleDepth);
        blurred += computeRawAo(sampleUv, sampleOrigin);
    }
    blurred *= 0.2;

    return pow(saturate(blurred), ubo.effectParams.z);
}
