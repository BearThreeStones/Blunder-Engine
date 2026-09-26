// glTF COLOR_0 as albedo tint. SE-world snow/foliage packs a coverage
// mask in one channel (R=B=0, G varies). RGB multiply of that is lime/black.
// Broadcast the mask to grayscale on untextured draws; ignore it on textured
// meshes (paper-shader data). True RGB vertex color still multiplies.
//
// Paths (DUMMY-path-*) pack coverage in A with RGB exactly 0. That is not a
// G-mask (mx-mn == 0), so a straight RGB multiply zeros the dummy 0.8 factor.

bool packedSingleChannelMask(float3 vc)
{
    float mx = max(vc.r, max(vc.g, vc.b));
    float mn = min(vc.r, min(vc.g, vc.b));
    return mn < 0.02 && (mx - mn) > 0.04;
}

bool vertexRgbIsCoverageOnly(float3 vc)
{
    // Paths: RGB is 0. Snow G-mask low bin is ~0.005 — leave that alone so
    // packed-mask snow rims do not become opaque A=1 albedo.
    return max(vc.r, max(vc.g, vc.b)) < 0.001;
}

float3 applyVertexColorToAlbedo(float3 albedo, float4 vc, float hasAlbedoMap,
                                float skipVertexTint)
{
    // Paper / roughness-only cards pack COLOR_0 that is not a G-mask
    // (often RGB~0 with coverage in A). RGB multiply zeros mint albedo.
    if (hasAlbedoMap > 0.5 && skipVertexTint > 0.5)
    {
        return albedo;
    }
    if (packedSingleChannelMask(vc.rgb))
    {
        if (hasAlbedoMap > 0.5)
        {
            return albedo;
        }
        float cover = max(vc.r, max(vc.g, vc.b));
        return albedo * cover;
    }
    if (vertexRgbIsCoverageOnly(vc.rgb))
    {
        if (hasAlbedoMap > 0.5)
        {
            return albedo;
        }
        return albedo * vc.a;
    }
    return albedo * vc.rgb;
}

void clipPackedVertexColorCoverage(float4 vc, float hasAlbedoMap)
{
    if (hasAlbedoMap > 0.5)
    {
        return;
    }
    if (packedSingleChannelMask(vc.rgb))
    {
        clip(max(vc.r, max(vc.g, vc.b)) - 0.04);
    }
    else if (vertexRgbIsCoverageOnly(vc.rgb))
    {
        clip(vc.a - 0.04);
    }
}
