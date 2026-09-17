// Shared exponential froxel mapping for the fill compute pass and the
// clustered GBuffer lighting pass. Keep in lockstep with froxel_grid.h.

static const uint kFroxelTileSizePx = 64;
static const uint kFroxelZSlices = 32;
static const uint kFroxelLightsPerCell = 64;
static const uint kMaxClusteredLights = 256;

float viewZToSliceF(float viewZ, float nearZ, float farZ, uint slices)
{
    float n = max(nearZ, 1e-4);
    float f = max(farZ, n + 1e-4);
    float z = max(viewZ, n);
    float t = log2(z / n) / log2(f / n);
    t = saturate(t);
    t = min(t, 0.999999);
    return t * float(max(slices, 1u));
}

uint viewZToSlice(float viewZ, float nearZ, float farZ, uint slices)
{
    uint n = max(slices, 1u);
    uint idx = uint(viewZToSliceF(viewZ, nearZ, farZ, n));
    return min(idx, n - 1u);
}

float sliceToViewZ(float slice, float nearZ, float farZ, uint slices)
{
    float n = max(nearZ, 1e-4);
    float f = max(farZ, n + 1e-4);
    float count = float(max(slices, 1u));
    float t = saturate(slice / count);
    return n * exp2(t * log2(f / n));
}

uint froxelIndex(uint tileX, uint tileY, uint slice, uint tilesX, uint tilesY)
{
    return (slice * tilesY + tileY) * tilesX + tileX;
}

float3 unprojectView(float4x4 invProjection, float2 ndc, float clipZ)
{
    float4 view = mul(invProjection, float4(ndc, clipZ, 1.0));
    view /= max(abs(view.w), 1e-8);
    return view.xyz;
}

float3 viewPointAtDepth(float4x4 invProjection, float2 ndc, float viewZ)
{
    float3 nearP = unprojectView(invProjection, ndc, 0.0);
    float3 farP = unprojectView(invProjection, ndc, 1.0);
    float nearZ = -nearP.z;
    float farZ = -farP.z;
    float denom = farZ - nearZ;
    float t = abs(denom) > 1e-6 ? (viewZ - nearZ) / denom : 0.0;
    return lerp(nearP, farP, t);
}

void froxelViewAabb(float4x4 invProjection, uint tileX, uint tileY, uint slice,
                    uint widthPx, uint heightPx, uint tileSize, uint slices,
                    float nearZ, float farZ, out float3 bmin, out float3 bmax)
{
    float x0 = float(tileX * tileSize);
    float y0 = float(tileY * tileSize);
    float x1 = min(float((tileX + 1u) * tileSize), float(widthPx));
    float y1 = min(float((tileY + 1u) * tileSize), float(heightPx));
    float w = float(widthPx);
    float h = float(heightPx);
    float2 ndc[4];
    ndc[0] = float2(x0 / w * 2.0 - 1.0, y0 / h * 2.0 - 1.0);
    ndc[1] = float2(x1 / w * 2.0 - 1.0, y0 / h * 2.0 - 1.0);
    ndc[2] = float2(x0 / w * 2.0 - 1.0, y1 / h * 2.0 - 1.0);
    ndc[3] = float2(x1 / w * 2.0 - 1.0, y1 / h * 2.0 - 1.0);
    float z0 = sliceToViewZ(float(slice), nearZ, farZ, slices);
    float z1 = sliceToViewZ(float(slice + 1u), nearZ, farZ, slices);
    bmin = float3(1e30, 1e30, 1e30);
    bmax = float3(-1e30, -1e30, -1e30);
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float3 p0 = viewPointAtDepth(invProjection, ndc[i], z0);
        float3 p1 = viewPointAtDepth(invProjection, ndc[i], z1);
        bmin = min(bmin, min(p0, p1));
        bmax = max(bmax, max(p0, p1));
    }
}

bool sphereOverlapsAabb(float3 center, float radius, float3 bmin, float3 bmax)
{
    float3 closest = clamp(center, bmin, bmax);
    float3 d = center - closest;
    return dot(d, d) <= radius * radius;
}

float occupancyHeatColorT(uint count)
{
    return saturate(float(count) / float(kFroxelLightsPerCell));
}

float3 occupancyHeatColor(uint count)
{
    float t = occupancyHeatColorT(count);
    float3 cold = float3(0.05, 0.15, 0.55);
    float3 mid = float3(0.15, 0.75, 0.25);
    float3 hot = float3(0.95, 0.15, 0.05);
    if (count == 0u)
    {
        return float3(0.04, 0.04, 0.06);
    }
    if (t < 0.5)
    {
        return lerp(cold, mid, t * 2.0);
    }
    return lerp(mid, hot, (t - 0.5) * 2.0);
}
