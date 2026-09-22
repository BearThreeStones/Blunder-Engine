// Deferred Render Path G-buffer geometry (GPU-skinned meshes).
//
// Same planes and material rules as gbuffer.slang; the vertex stage consumes
// the same Matrix Palette UBO as pbr_skinned.slang (binding 1 here because the
// G-buffer set 0 has no shadow map).

#include "vertex_color.slang"

struct GBufferUniformData
{
    float4x4 model;
    float4x4 view;
    float4x4 projection;
    float4x4 normalMatrix;
    float4 baseColorFactor;
    float4 materialFlags;            // x unlit, y albedo map, z roughness-only MR
    float4 metallicRoughnessFactors; // x metallic, y roughness, z cutoff, w alpha mode
    float4 pbrTextureFlags;          // x mr map, y normal map, z ao map, w two-sided
    uint4 bindlessTextureIndices;    // x base, y mr, z normal, w ao
    uint4 receiver;                  // x = draw slot
};

[[vk::binding(0, 0)]]
ConstantBuffer<GBufferUniformData> ubo;

static const uint kMaxSkinJoints = 128;

struct BonePaletteData
{
    float4x4 jointMatrices[kMaxSkinJoints];
};

[[vk::binding(1, 0)]]
ConstantBuffer<BonePaletteData> bonePalette;

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
    int4 inJointIndices;
    [[vk::location(5)]]
    float4 inWeights;
    [[vk::location(6)]]
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
    output.uv = input.inUv;
    output.color = input.inColor;
    return output;
}

float3 getNormalFromMap(VertexOutput input, float4 sampledNormal)
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
    float3 mapNormal = sampledNormal.xyz * 2.0 - 1.0;
    return normalize(mul(tbn, mapNormal));
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

[shader("fragment")]
GBufferOutput fragmentMain(VertexOutput input)
{
    float4 sampledAlbedo = sampleBindless(ubo.bindlessTextureIndices.x, input.uv);
    float4 sampledMr = sampleBindless(ubo.bindlessTextureIndices.y, input.uv);
    float4 sampledNormal = sampleBindless(ubo.bindlessTextureIndices.z, input.uv);
    float4 sampledAo = sampleBindless(ubo.bindlessTextureIndices.w, input.uv);
    float4 baseSample = ubo.materialFlags.y > 0.5 ? sampledAlbedo : float4(1.0, 1.0, 1.0, 1.0);
    float3 albedo = applyVertexColorToAlbedo(
        baseSample.rgb * ubo.baseColorFactor.rgb, input.color.rgb,
        ubo.materialFlags.y);
    float alpha = baseSample.a * ubo.baseColorFactor.a * input.color.a;
    clipPackedVertexColorCoverage(input.color.rgb, ubo.materialFlags.y);

    if (ubo.metallicRoughnessFactors.w > 0.5 &&
        ubo.metallicRoughnessFactors.w < 1.5)
    {
        clip(alpha - ubo.metallicRoughnessFactors.z);
    }

    float metallic = ubo.metallicRoughnessFactors.x;
    float roughness = ubo.metallicRoughnessFactors.y;
    if (ubo.pbrTextureFlags.x > 0.5)
    {
        roughness *= sampledMr.g;
        if (ubo.materialFlags.z < 0.5)
        {
            metallic *= sampledMr.b;
        }
    }

    float ao = ubo.pbrTextureFlags.z > 0.5 ? sampledAo.r : 1.0;
    float3 normal = getNormalFromMap(input, sampledNormal);

    uint receiver = ubo.receiver.x & kReceiverSlotMask;
    if (ubo.materialFlags.x > 0.5)
    {
        receiver |= kReceiverUnlitBit;
    }
    if (ubo.pbrTextureFlags.w > 0.5)
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
