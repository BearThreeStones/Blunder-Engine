#include "gpu_driven_common.slang"
#include "vertex_color.slang"

struct GBufferUniformData
{
    float4x4 model;
    float4x4 view;
    float4x4 projection;
    float4x4 normalMatrix;
    float4 baseColorFactor;
    float4 materialFlags;  // x unlit, y albedo map, z roughness-only MR
    float4 metallicRoughnessFactors;
    float4 pbrTextureFlags;
    uint4 bindlessTextureIndices;
    uint4 receiver;
};

[[vk::binding(0, 0)]]
ConstantBuffer<GBufferUniformData> ubo;

[[vk::binding(1, 0)]]
StructuredBuffer<GpuDrivenInstanceGpu> instances;

[[vk::binding(2, 0)]]
StructuredBuffer<uint> compactIds;

[[vk::binding(0, 1)]]
Texture2D<float4> bindlessTextures[];

[[vk::binding(1, 1)]]
SamplerState bindlessSamplers[];

static const uint kReceiverSlotMask = 0x3FFFu;
static const uint kReceiverUnlitBit = 0x4000u;
static const uint kReceiverTwoSidedBit = 0x8000u;

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
    float3 worldTangent;
    [[vk::location(3)]]
    float tangentSign;
    [[vk::location(4)]]
    nointerpolation uint instanceId;
    [[vk::location(5)]]
    float4 color;
};

struct GBufferOutput
{
    float4 albedoAo : SV_Target0;
    float4 normalMetalRough : SV_Target1;
    uint receiver : SV_Target2;
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
    output.uv = input.inUv;
    output.instanceId = instIndex;
    output.color = input.inColor;
    return output;
}

float2 octSign(float2 v)
{
    return float2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
}

float2 encodeOctNormal(float3 n)
{
    n /= max(abs(n.x) + abs(n.y) + abs(n.z), 1e-6);
    float2 p = n.z >= 0.0 ? n.xy : (1.0 - abs(n.yx)) * octSign(n.xy);
    return p * 0.5 + 0.5;
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

[shader("fragment")]
GBufferOutput fragmentMain(VertexOutput input)
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
        if (inst.materialFlags.z < 0.5)
        {
            metallic *= sampledMr.b;
        }
    }

    float ao = inst.pbrTextureFlags.z > 0.5 ? sampledAo.r : 1.0;
    float3 normal = getNormalFromMap(input, inst, sampledNormal);
    uint receiver = inst.receiverId & kReceiverSlotMask;
    if (inst.materialFlags.x > 0.5)
    {
        receiver |= kReceiverUnlitBit;
    }
    if (inst.pbrTextureFlags.w > 0.5)
    {
        receiver |= kReceiverTwoSidedBit;
    }

    GBufferOutput output;
    output.albedoAo = float4(saturate(albedo), saturate(ao));
    output.normalMetalRough =
        float4(encodeOctNormal(normal), saturate(metallic), saturate(roughness));
    output.receiver = receiver;
    return output;
}
