// Shared 2x2 hardware comparison PCF for directional VSM pages, point cubes,
// and spot maps. Bindings stay in the including shader.

float sampleCmp2x2(Texture2D<float> map, SamplerComparisonState samp, float2 uv,
                   float refDepth, float texel)
{
    float shadow = 0.0;
    shadow += map.SampleCmp(samp, uv + float2(-texel, -texel), refDepth);
    shadow += map.SampleCmp(samp, uv + float2(texel, -texel), refDepth);
    shadow += map.SampleCmp(samp, uv + float2(-texel, texel), refDepth);
    shadow += map.SampleCmp(samp, uv + float2(texel, texel), refDepth);
    return shadow * 0.25;
}

float sampleCmpArray2x2(Texture2DArray<float> map, SamplerComparisonState samp,
                        float2 uv, uint layer, float refDepth, float texel)
{
    float shadow = 0.0;
    shadow += map.SampleCmp(samp, float3(uv + float2(-texel, -texel), layer), refDepth);
    shadow += map.SampleCmp(samp, float3(uv + float2(texel, -texel), layer), refDepth);
    shadow += map.SampleCmp(samp, float3(uv + float2(-texel, texel), layer), refDepth);
    shadow += map.SampleCmp(samp, float3(uv + float2(texel, texel), layer), refDepth);
    return shadow * 0.25;
}

float sampleCmpCube2x2(TextureCubeArray<float> map, SamplerComparisonState samp,
                       float3 dir, uint cubeIndex, float refDepth, float texel)
{
    float3 n = normalize(dir);
    float3 up = abs(n.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(0.0, 1.0, 0.0);
    float3 t = normalize(cross(up, n));
    float3 b = cross(n, t);
    float shadow = 0.0;
    shadow += map.SampleCmp(samp, float4(normalize(n + (-t - b) * texel), cubeIndex), refDepth);
    shadow += map.SampleCmp(samp, float4(normalize(n + (t - b) * texel), cubeIndex), refDepth);
    shadow += map.SampleCmp(samp, float4(normalize(n + (-t + b) * texel), cubeIndex), refDepth);
    shadow += map.SampleCmp(samp, float4(normalize(n + (t + b) * texel), cubeIndex), refDepth);
    return shadow * 0.25;
}

float vsmClipmapRadius(uint level)
{
    return exp2(float(level + 1u));
}

float sampleVsmPages(float3 worldPos, float4x4 lightView, float4 originLevels,
                     float4 vsmParams, Texture2DArray<float> pages,
                     StructuredBuffer<uint> pageTable, SamplerComparisonState samp,
                     float bias)
{
    if (vsmParams.w < 0.5)
    {
        return 1.0;
    }
    float3 lightPos = mul(lightView, float4(worldPos, 1.0)).xyz;
    uint firstLevel = uint(originLevels.w + 0.5);
    uint lastLevel = uint(vsmParams.x + 0.5);
    uint pagesPerAxis = uint(vsmParams.y + 0.5);
    if (pagesPerAxis == 0u)
    {
        pagesPerAxis = 128u;
    }
    uint chosen = lastLevel + 1u;
    uint physical = 0xFFFFFFFFu;
    for (uint level = firstLevel; level <= lastLevel; ++level)
    {
        float radius = vsmClipmapRadius(level);
        float2 uv = lightPos.xy / (2.0 * radius) + 0.5;
        if (uv.x < 0.0 || uv.x >= 1.0 || uv.y < 0.0 || uv.y >= 1.0)
        {
            continue;
        }
        float2 scaled = uv * float(pagesPerAxis);
        uint pageX = uint(scaled.x);
        uint pageY = uint(scaled.y);
        uint local = level - firstLevel;
        uint virt = local * pagesPerAxis * pagesPerAxis + pageY * pagesPerAxis + pageX;
        if (virt >= 81920u)
        {
            continue;
        }
        uint slot = pageTable[virt];
        if (slot == 0xFFFFFFFFu)
        {
            continue;
        }
        chosen = level;
        physical = slot;
        break;
    }
    if (chosen > lastLevel)
    {
        return 1.0;
    }
    float radius = vsmClipmapRadius(chosen);
    float2 uv = lightPos.xy / (2.0 * radius) + 0.5;
    float2 scaled = uv * float(pagesPerAxis);
    float2 pageUv = frac(scaled);
    pageUv.y = 1.0 - pageUv.y;
    float z = saturate((-lightPos.z - 0.05) / (2048.0 - 0.05));
    float refDepth = z - bias;
    float texel = vsmParams.z;
    return sampleCmpArray2x2(pages, samp, pageUv, physical, refDepth, texel);
}

float samplePointCubeShadow(GpuSceneLight light, float3 worldPos,
                            TextureCubeArray<float> cubes, SamplerComparisonState samp,
                            float bias, float texel)
{
    float3 toPoint = worldPos - light.positionType.xyz;
    float dist = length(toPoint);
    float range = light.emitRange.w;
    if (range <= 1e-5 || dist >= range)
    {
        return 1.0;
    }
    uint slot = uint(light.axisX.w - 2.0 + 0.5);
    float far = max(range, 0.06);
    float nearP = 0.05;
    float z = max(dist, nearP);
    float a = far / (far - nearP);
    float b = -(far * nearP) / (far - nearP);
    float clipZ = a + b / z;
    return sampleCmpCube2x2(cubes, samp, toPoint, slot, clipZ - bias, texel);
}

float sampleSpotShadow(GpuSceneLight light, float3 worldPos, float4x4 spotVP,
                       Texture2DArray<float> spots, SamplerComparisonState samp,
                       float bias, float texel, uint slot)
{
    float4 clip = mul(spotVP, float4(worldPos, 1.0));
    if (clip.w <= 0.0)
    {
        return 1.0;
    }
    float3 ndc = clip.xyz / clip.w;
    float2 uv = ndc.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
    {
        return 1.0;
    }
    return sampleCmpArray2x2(spots, samp, uv, slot, ndc.z - bias, texel);
}

float sampleLightShadowTerm(GpuSceneLight light, float3 worldPos, float directional,
                            Texture2D<float> classicMap, SamplerComparisonState samp,
                            float4x4 classicVP, float4 shadowParams,
                            float4x4 vsmLightView, float4 vsmOriginLevels, float4 vsmParams,
                            Texture2DArray<float> vsmPages, StructuredBuffer<uint> pageTable,
                            TextureCubeArray<float> pointCubes, Texture2DArray<float> spotMaps,
                            float4x4 spotViewProjections[8])
{
    float code = light.axisX.w;
    if (code < 0.5)
    {
        return 1.0;
    }
    if (code < 1.5)
    {
        if (vsmParams.w > 0.5)
        {
            return sampleVsmPages(worldPos, vsmLightView, vsmOriginLevels, vsmParams,
                                  vsmPages, pageTable, samp, shadowParams.x);
        }
        return directional;
    }
    if (code < 9.5)
    {
        return samplePointCubeShadow(light, worldPos, pointCubes, samp, shadowParams.x,
                                     1.0 / 512.0);
    }
    uint slot = uint(code - 10.0 + 0.5);
    if (slot >= 8u)
    {
        return 1.0;
    }
    return sampleSpotShadow(light, worldPos, spotViewProjections[slot], spotMaps, samp,
                            shadowParams.x, 1.0 / 512.0, slot);
}
