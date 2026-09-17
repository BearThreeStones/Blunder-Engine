// Camera-aligned volumetric fog (Unreal CalculateGridZParams family).
// Keep in lockstep with volumetric_fog_math.h.

static const uint kVolumetricFogSliceCount = 64;
static const float kVolumetricFogS = 32.0;
static const uint kMaxVolumetricFogLocalLights = 8;
static const float kVolumetricFogLocalSourceRadius = 1.25;

struct FogLocalLight
{
    float4 positionType;  // xyz world, w 1=point 2=spot
    float4 colorFlags;    // rgb color*intensity, w illuminate
    float4 emitRange;     // xyz world emit, w range
    float4 coneArea;      // x inner cos, y outer cos
    float4 axisX;         // unused in fog (shadow code stays 0)
};

struct VolumetricFogUniforms
{
    float4x4 invViewProj;
    float4x4 viewProj;
    float4x4 prevViewProj;
    float4 cameraPosFar;      // xyz camera world, w volume far
    float4 gridZParams;       // xyz X/Y/S, w near
    float4 densityParams;     // x density, y falloff, z fogHeight, w local source radius
    float4 fogAlbedoG;        // rgb albedo, w g
    float4 lightColor;        // rgb illuminance, w hasDirectional
    float4 lightDir;          // xyz world emit (-Z), w unused
    float4 gridSize;          // xyz counts, w unused
    float4 jitterTemporal;    // xy Halton jitter, z current mix, w historyValid
    float4 localLightCount;   // x packed point/spot count
    FogLocalLight localLights[kMaxVolumetricFogLocalLights];
};

float viewZFromSlice(float slice, float3 gridZParams)
{
    return (exp2(slice / gridZParams.z) - gridZParams.y) / gridZParams.x;
}

float sliceFromViewZ(float viewZ, float3 gridZParams)
{
    return log2(max(viewZ * gridZParams.x + gridZParams.y, 1e-6)) * gridZParams.z;
}

float heightDensity(float density, float falloff, float worldZ, float fogHeight)
{
    float dz = max(worldZ - fogHeight, 0.0);
    return density * exp2(-falloff * dz);
}

float henyeyGreenstein(float cosTheta, float g)
{
    float g2 = g * g;
    float denom = 1.0 + g2 - 2.0 * g * cosTheta;
    return (1.0 - g2) / (12.5663706144 * denom * sqrt(max(denom, 1e-6)));
}

float fogPunctualAtten(float dist, float range, float sourceRadius)
{
    if (range <= 1e-6)
    {
        return 0.0;
    }
    float d = max(dist, max(sourceRadius, 1e-4));
    if (d >= range)
    {
        return 0.0;
    }
    float t = 1.0 - d / range;
    return t * t;
}

float3 fogLocalLightDirAndAtten(FogLocalLight light, float3 worldPos, float sourceRadius,
                                out float atten)
{
    float type = light.positionType.w;
    float3 emit = light.emitRange.xyz;
    float emitLen = length(emit);
    emit = emitLen > 1e-6 ? emit / emitLen : float3(0.0, 0.0, -1.0);
    atten = 1.0;
    float3 toLight = light.positionType.xyz - worldPos;
    float dist = length(toLight);
    float3 L = dist > 1e-6 ? toLight / dist : float3(0.0, 0.0, 1.0);
    atten = fogPunctualAtten(dist, light.emitRange.w, sourceRadius);
    if (type > 1.5 && type < 2.5)
    {
        float cd = dot(-L, emit);
        float innerCos = light.coneArea.x;
        float outerCos = light.coneArea.y;
        atten *= saturate((cd - outerCos) / max(innerCos - outerCos, 1e-5));
    }
    return L;
}

float3 froxelWorldPos(uint3 id, float3 gridSize, float2 jitter, float3 gridZParams,
                      float4x4 invViewProj, float3 cameraPos)
{
    float3 uvw = (float3(id) + 0.5 + float3(jitter, 0.0)) / gridSize;
    float viewZ = viewZFromSlice(float(id.z) + 0.5, gridZParams);
    float2 ndc = uvw.xy * 2.0 - 1.0;
    // glm::perspectiveZO: NDC z=0 is near. Unprojecting z=1 (camera far) after
    // Viewport far 100000 makes invViewProj ill-conditioned so froxels miss
    // the courtyard even when gridZParams still use Fog Unique viewDistance.
    float4 nearH = mul(invViewProj, float4(ndc, 0.0, 1.0));
    float3 worldNear = nearH.xyz / max(nearH.w, 1e-6);
    float3 dir = normalize(worldNear - cameraPos);
    return cameraPos + dir * viewZ;
}

float froxelSliceDepth(uint slice, float3 gridZParams)
{
    float z0 = viewZFromSlice(float(slice), gridZParams);
    float z1 = viewZFromSlice(float(slice) + 1.0, gridZParams);
    return max(z1 - z0, 1e-4);
}
