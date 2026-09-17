#include "froxel_common.slang"

struct GpuSceneLight
{
    float4 positionType;
    float4 colorFlags;
    float4 emitRange;
    float4 coneArea;
    float4 axisX;
};

struct FroxelFillUniformData
{
    float4x4 view;
    float4x4 invProjection;
    float4 screen;   // width, height, tilesX, tilesY
    float4 zParams;  // near, far, slices, tileSize
    float4 lightCount; // x = clustered point/spot count
};

[[vk::binding(0, 0)]]
ConstantBuffer<FroxelFillUniformData> ubo;

[[vk::binding(1, 0)]]
StructuredBuffer<GpuSceneLight> clusteredLights;

[[vk::binding(2, 0)]]
RWStructuredBuffer<uint> froxelLightIndices;

[[vk::binding(3, 0)]]
RWStructuredBuffer<uint> froxelLightCount;

[[vk::binding(4, 0)]]
RWStructuredBuffer<uint> froxelOverflow;

[shader("compute")]
[numthreads(4, 4, 4)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint tilesX = uint(ubo.screen.z);
    uint tilesY = uint(ubo.screen.w);
    uint slices = uint(ubo.zParams.z);
    if (id.x >= tilesX || id.y >= tilesY || id.z >= slices)
    {
        return;
    }

    uint widthPx = uint(ubo.screen.x);
    uint heightPx = uint(ubo.screen.y);
    uint tileSize = uint(ubo.zParams.w);
    float nearZ = ubo.zParams.x;
    float farZ = ubo.zParams.y;
    uint lightCount = uint(ubo.lightCount.x);

    float3 bmin;
    float3 bmax;
    froxelViewAabb(ubo.invProjection, id.x, id.y, id.z, widthPx, heightPx, tileSize,
                   slices, nearZ, farZ, bmin, bmax);

    uint cell = froxelIndex(id.x, id.y, id.z, tilesX, tilesY);
    uint written = 0;
    uint dropped = 0;
    for (uint li = 0; li < kMaxClusteredLights; ++li)
    {
        if (li >= lightCount)
        {
            break;
        }
        GpuSceneLight light = clusteredLights[li];
        float type = light.positionType.w;
        // Point = 1, Spot = 2. Directional (0) and area (3) never enter.
        if (type < 0.5 || type > 2.5)
        {
            continue;
        }
        float3 worldPos = light.positionType.xyz;
        float3 viewPos = mul(ubo.view, float4(worldPos, 1.0)).xyz;
        float radius = light.emitRange.w;
        if (!sphereOverlapsAabb(viewPos, radius, bmin, bmax))
        {
            continue;
        }
        if (written < kFroxelLightsPerCell)
        {
            froxelLightIndices[cell * kFroxelLightsPerCell + written] = li;
            written += 1;
        }
        else
        {
            dropped += 1;
        }
    }
    froxelLightCount[cell] = written;
    if (dropped > 0)
    {
        uint dummy = 0;
        InterlockedAdd(froxelOverflow[0], dropped, dummy);
    }
}
