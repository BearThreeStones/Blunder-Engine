namespace Blunder;

/// <summary>
/// Immediate-mode debug lines in metres, Z-up. Not <see cref="Debug"/> logs.
/// </summary>
public static class Draw
{
    public const float DefaultWidthPx = 1f;

    public static void Line(Vec3 a, Vec3 b, Vec3 color, float duration = 0f,
                            float alpha = 1f, float widthPx = DefaultWidthPx)
    {
        if (!Native.IsRegistered)
        {
            return;
        }

        _ = Native.blunder_debug_draw_line(
            a.X, a.Y, a.Z, b.X, b.Y, b.Z, color.X, color.Y, color.Z, alpha,
            duration, widthPx);
    }

    public static void Ray(Vec3 origin, Vec3 direction, Vec3 color,
                           float duration = 0f, float alpha = 1f,
                           float widthPx = DefaultWidthPx)
    {
        if (!Native.IsRegistered)
        {
            return;
        }

        _ = Native.blunder_debug_draw_ray(
            origin.X, origin.Y, origin.Z, direction.X, direction.Y, direction.Z,
            color.X, color.Y, color.Z, alpha, duration, widthPx);
    }

    public static void Arrow(Vec3 from, Vec3 to, Vec3 color, float duration = 0f,
                             float alpha = 1f, float widthPx = DefaultWidthPx)
    {
        if (!Native.IsRegistered)
        {
            return;
        }

        _ = Native.blunder_debug_draw_arrow(
            from.X, from.Y, from.Z, to.X, to.Y, to.Z, color.X, color.Y, color.Z,
            alpha, duration, widthPx);
    }

    public static void WireBox(Vec3 center, Vec3 size, Vec3 color,
                               float duration = 0f, float alpha = 1f,
                               float widthPx = DefaultWidthPx)
    {
        if (!Native.IsRegistered)
        {
            return;
        }

        _ = Native.blunder_debug_draw_wire_box(
            center.X, center.Y, center.Z, size.X, size.Y, size.Z,
            color.X, color.Y, color.Z, alpha, duration, widthPx);
    }

    public static void WireSphere(Vec3 center, float radius, Vec3 color,
                                  float duration = 0f, float alpha = 1f,
                                  float widthPx = DefaultWidthPx)
    {
        if (!Native.IsRegistered)
        {
            return;
        }

        _ = Native.blunder_debug_draw_wire_sphere(
            center.X, center.Y, center.Z, radius, color.X, color.Y, color.Z,
            alpha, duration, widthPx);
    }

    public static void WireCapsule(Vec3 a, Vec3 b, float radius, Vec3 color,
                                   float duration = 0f, float alpha = 1f,
                                   float widthPx = DefaultWidthPx)
    {
        if (!Native.IsRegistered)
        {
            return;
        }

        _ = Native.blunder_debug_draw_wire_capsule(
            a.X, a.Y, a.Z, b.X, b.Y, b.Z, radius, color.X, color.Y, color.Z,
            alpha, duration, widthPx);
    }

    public static void Cross(Vec3 point, float size, Vec3 color,
                             float duration = 0f, float alpha = 1f,
                             float widthPx = DefaultWidthPx)
    {
        if (!Native.IsRegistered)
        {
            return;
        }

        _ = Native.blunder_debug_draw_cross(
            point.X, point.Y, point.Z, size, color.X, color.Y, color.Z, alpha,
            duration, widthPx);
    }

    /// <summary>Standalone Player only. Editor viewports always draw.</summary>
    public static void SetInGameEnabled(bool enabled)
    {
        if (!Native.IsRegistered)
        {
            return;
        }

        _ = Native.blunder_debug_draw_set_ingame_enabled(enabled ? 1 : 0);
    }

    public static bool IsInGameEnabled()
    {
        if (!Native.IsRegistered)
        {
            return false;
        }

        _ = Native.blunder_debug_draw_get_ingame_enabled(out int enabled);
        return enabled != 0;
    }
}
