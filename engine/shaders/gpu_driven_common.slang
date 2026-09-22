// Packed GPU-driven instance / meshlet records. Must match
// engine/src/runtime/function/render/gpu_driven/gpu_driven_types.h.

struct GpuDrivenInstanceGpu
{
    float4x4 world;
    float4x4 normalMatrix;
    uint4 bindlessTextureIndices;
    float4 baseColorFactor;
    float4 metallicRoughnessFactors;
    float4 pbrTextureFlags;
    float4 materialFlags;  // x unlit, y albedo map, z roughness-only MR
    uint receiverId;
    uint flags;
    uint meshletOffset;
    uint meshletCount;
    uint lightMask;
    uint pad0;
    uint pad1;
    uint pad2;
};

struct GpuDrivenMeshletGpu
{
    float4 sphere;
    float4 cone;
    uint instanceIndex;
    uint firstIndex;
    uint indexCount;
    uint flags;
    uint vertexOffset;
    uint vertexCount;
    uint triangleOffset;
    uint triangleCount;
};

struct GpuDrivenCullUniforms
{
    float4x4 viewProjection;
    float4 cameraPosition;
    float4 frustumPlanes[6];
    uint meshletCount;
    uint hizEnabled;
    uint hizMipCount;
    uint skipCone;
    uint compactCommands;
    uint pad0;
    uint pad1;
    uint pad2;
    float4 hizSize;
};

struct GpuDrivenEmitUniforms
{
    uint uniqueCount;
    uint compactCommands;
    uint pad0;
    uint pad1;
};

struct DrawIndexedIndirectCommand
{
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    uint vertexOffset;
    uint firstInstance;
    uint pad0;
    uint pad1;
    uint pad2;
};

static const uint kGpuDrivenFlagTwoSided = 1u;
static const uint kGpuDrivenFlagSkipCone = 2u;
static const uint kGpuDrivenMeshletBatchShift = 16u;

// Packed MeshVertex / Vertex. Must match mesh_asset.h and vulkan_buffer.h.
static const uint kMeshVertexStride = 64u;
static const uint kMeshVertexNormalOffset = 12u;
static const uint kMeshVertexUvOffset = 24u;
static const uint kMeshVertexTangentOffset = 32u;
static const uint kMeshVertexColorOffset = 48u;
