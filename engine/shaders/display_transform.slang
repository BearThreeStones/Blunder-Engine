// Viewport display transform. The offscreen colour target is
// R8G8B8A8_UNORM with no sRGB image view (offscreen_render_target.h), and Slint
// presents it as-is, so shading has to apply the sRGB OETF itself. Writing
// linear radiance straight in costs roughly a 2.2 gamma: a courtyard wall lit to
// 0.06 landed on 16/255 instead of the 71/255 it should display as.
//
// Anything that writes the presented viewport colour encodes on the way out, and
// anything that blends against an already-presented colour decodes first.

/// IEC 61966-2-1 OETF.
float3 linearToSrgb(float3 c)
{
    float3 clamped = saturate(c);
    float3 lo = clamped * 12.92;
    float3 hi = 1.055 * pow(max(clamped, 1e-5), 1.0 / 2.4) - 0.055;
    return lerp(lo, hi, step(0.0031308, clamped));
}

/// IEC 61966-2-1 EOTF.
float3 srgbToLinear(float3 c)
{
    float3 clamped = saturate(c);
    float3 lo = clamped / 12.92;
    float3 hi = pow((clamped + 0.055) / 1.055, 2.4);
    return lerp(lo, hi, step(0.04045, clamped));
}

/// Reinhard. Keeps punctual highlights off the UNORM ceiling before encoding.
float3 tonemapReinhard(float3 c)
{
    return c / (c + 1.0);
}
