#include "gpu_driven_common.slang"
#include "vertex_color.slang"

// Deferred G-buffer mesh path (VK_EXT_mesh_shader). Task/mesh match
// pbr_mesh.slang; fragment matches gbuffer_gpu_driven.slang MRT outputs.
// Payload stays instance-index + meshlet local offset only (no world matrices).

struct GBufferUniformData
{
    float4x4 model;
    float4x4 view;
    float4x4 projection;
    float4x4 normalMatrix;
    float4 baseColorFactor;
    float4 materialFlags;
    float4 metallicRoughnessFactors;
    float4 pbrTextureFlags;
    uint4 bindlessTextureIndices;
    uint4 receiver;
};

struct MeshPayload
{
    uint meshletIndex;
    uint compactFirst;
};

[[vk::binding(0, 0)]]
ConstantBuffer<GBufferUniformData> ubo;

[[vk::binding(1, 0)]]
StructuredBuffer<GpuDrivenInstanceGpu> instances;

[[vk::binding(2, 0)]]
StructuredBuffer<DrawIndexedIndirectCommand> commands;

[[vk::binding(3, 0)]]
ByteAddressBuffer vertexBytes;

[[vk::binding(4, 0)]]
ByteAddressBuffer meshletVertices;

[[vk::binding(5, 0)]]
ByteAddressBuffer meshletTriangles;

[[vk::binding(6, 0)]]
StructuredBuffer<GpuDrivenMeshletGpu> meshlets;

[[vk::binding(7, 0)]]
StructuredBuffer<uint> compactIds;

[[vk::binding(0, 1)]]
Texture2D<float4> bindlessTextures[];

[[vk::binding(1, 1)]]
SamplerState bindlessSamplers[];

static const uint kReceiverSlotMask = 0x3FFFu;
static const uint kReceiverUnlitBit = 0x4000u;
static const uint kReceiverTwoSidedBit = 0x8000u;

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

float3 loadPosition(uint vertexIndex)
{
    return asfloat(vertexBytes.Load3(vertexIndex * kMeshVertexStride));
}

float3 loadNormal(uint vertexIndex)
{
    return asfloat(vertexBytes.Load3(vertexIndex * kMeshVertexStride + kMeshVertexNormalOffset));
}

float2 loadUv(uint vertexIndex)
{
    return asfloat(vertexBytes.Load2(vertexIndex * kMeshVertexStride + kMeshVertexUvOffset));
}

float4 loadTangent(uint vertexIndex)
{
    return asfloat(vertexBytes.Load4(vertexIndex * kMeshVertexStride + kMeshVertexTangentOffset));
}

float4 loadColor(uint vertexIndex)
{
    return asfloat(vertexBytes.Load4(vertexIndex * kMeshVertexStride + kMeshVertexColorOffset));
}

uint loadMeshletVertex(uint offset)
{
    return meshletVertices.Load(offset * 4u);
}

uint loadTriangleByte(uint byteOffset)
{
    uint word = meshletTriangles.Load((byteOffset / 4u) * 4u);
    uint shift = (byteOffset & 3u) * 8u;
    return (word >> shift) & 0xffu;
}

[shader("amplification")]
[numthreads(1, 1, 1)]
void taskMain(uint3 dtid : SV_DispatchThreadID)
{
    // Same surviving-count contract as pbr_mesh.slang (Phase 3).
    DrawIndexedIndirectCommand cmd = commands[dtid.x];
    MeshPayload payload;
    payload.meshletIndex = cmd.pad0;
    payload.compactFirst = cmd.firstInstance;
    uint n = cmd.instanceCount;
    if (n == 0u)
    {
        DispatchMesh(0, 1, 1, payload);
        return;
    }
    if (n > 16128u)
    {
        n = 16128u;
    }
    DispatchMesh(n, 1, 1, payload);
}

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(128, 1, 1)]
void meshMain(uint gtid : SV_GroupThreadID, uint3 gid : SV_GroupID,
              in payload MeshPayload payload, out vertices VertexOutput verts[64],
              out indices uint3 tris[124])
{
    GpuDrivenMeshletGpu meshlet = meshlets[payload.meshletIndex];
    uint vertCount = meshlet.vertexCount;
    uint triCount = meshlet.triangleCount;
    if (vertCount > 64u)
    {
        vertCount = 64u;
    }
    if (triCount > 124u)
    {
        triCount = 124u;
    }
    SetMeshOutputCounts(vertCount, triCount);

    uint instIndex = compactIds[payload.compactFirst + gid.x];
    GpuDrivenInstanceGpu inst = instances[instIndex];
    if (gtid < vertCount)
    {
        uint index = loadMeshletVertex(meshlet.vertexOffset + gtid);
        float3 pos = loadPosition(index);
        float3 nrm = loadNormal(index);
        float2 uv = loadUv(index);
        float4 tan = loadTangent(index);
        float4 worldPos = mul(inst.world, float4(pos, 1.0));
        float4 viewPos = mul(ubo.view, worldPos);
        verts[gtid].sv_position = mul(ubo.projection, viewPos);
        verts[gtid].worldNormal = normalize(mul((float3x3)inst.normalMatrix, nrm));
        verts[gtid].worldTangent = normalize(mul((float3x3)inst.normalMatrix, tan.xyz));
        verts[gtid].tangentSign = tan.w;
        verts[gtid].uv = uv;
        verts[gtid].color = loadColor(index);
        verts[gtid].instanceId = instIndex;
    }
    if (gtid < triCount)
    {
        uint base = meshlet.triangleOffset + gtid * 3u;
        tris[gtid] = uint3(loadTriangleByte(base), loadTriangleByte(base + 1u),
                           loadTriangleByte(base + 2u));
    }
}

float4 sampleBindless(uint index, float2 uv)
{
    uint i = NonUniformResourceIndex(index);
    return bindlessTextures[i].Sample(bindlessSamplers[i], uv);
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
        baseSample.rgb * inst.baseColorFactor.rgb, input.color,
        inst.materialFlags.y, inst.materialFlags.z);
    float alpha = baseSample.a * inst.baseColorFactor.a * input.color.a;
    clipPackedVertexColorCoverage(input.color, inst.materialFlags.y);

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
        if (inst.materialFlags.z > 0.5)
        {
            metallic = 0.0;
        }
        else
        {
            metallic *= sampledMr.b;
        }
    }
    if (inst.materialFlags.z > 0.5)
    {
        metallic = 0.0;
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
