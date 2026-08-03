namespace Blunder;

/// <summary>
/// Managed façade for native CINE segment enter/end state (C-ABI v7).
/// Pose snap and gameplay state transitions remain Behaviour responsibilities.
/// </summary>
public static class CineSegment
{
    public static bool Enter(bool suppressGameplayInput = false) =>
        Native.blunder_cine_enter(suppressGameplayInput ? 1 : 0) == Native.Ok;

    public static bool End() => Native.blunder_cine_end() == Native.Ok;

    public static bool IsInCine
    {
        get
        {
            if (Native.blunder_cine_is_in_cine(out int value) != Native.Ok)
            {
                return false;
            }

            return value != 0;
        }
    }

    public static bool IsGameplayInputSuppressed
    {
        get
        {
            if (Native.blunder_cine_is_gameplay_input_suppressed(out int value) != Native.Ok)
            {
                return false;
            }

            return value != 0;
        }
    }
}
