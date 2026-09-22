// glTF COLOR_0 as albedo tint. SE-world snow/foliage packs a coverage
// mask in one channel (R=B=0, G varies). RGB multiply of that is lime/black.
// Broadcast the mask to grayscale on untextured draws; ignore it on textured
// meshes (paper-shader data). True RGB vertex color still multiplies.

bool packedSingleChannelMask(float3 vc)
{
    float mx = max(vc.r, max(vc.g, vc.b));
    float mn = min(vc.r, min(vc.g, vc.b));
    return mn < 0.02 && (mx - mn) > 0.04;
}

float3 applyVertexColorToAlbedo(float3 albedo, float3 vc, float hasAlbedoMap,
                                float skipVertexTint)
{
    // Paper / roughness-only cards pack COLOR_0 that is not a G-mask
    // (often RGB~0 with coverage in A). RGB multiply zeros mint albedo.
    if (hasAlbedoMap > 0.5 && skipVertexTint > 0.5)
    {
        return albedo;
    }
    if (packedSingleChannelMask(vc))
    {
        if (hasAlbedoMap > 0.5)
        {
            return albedo;
        }
        float cover = max(vc.r, max(vc.g, vc.b));
        return albedo * cover;
    }
    return albedo * vc;
}

void clipPackedVertexColorCoverage(float3 vc, float hasAlbedoMap)
{
    if (hasAlbedoMap < 0.5 && packedSingleChannelMask(vc))
    {
        clip(max(vc.r, max(vc.g, vc.b)) - 0.04);
    }
}
