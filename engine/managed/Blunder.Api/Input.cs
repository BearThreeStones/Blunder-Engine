namespace Blunder;

/// <summary>
/// Polls Gameplay Actions for the current simulation frame.
/// Authoritative only in Player Play Mode; otherwise idle.
/// </summary>
public static unsafe class Input
{
    public static Vec2 GetMove()
    {
        float x = 0f, y = 0f;
        if (Native.blunder_gameplay_input_get_move(&x, &y) != Native.Ok)
        {
            throw new InvalidOperationException("gameplay_input_get_move failed");
        }
        return new Vec2(x, y);
    }

    public static bool WasJumpPressed()
    {
        int pressed = 0;
        if (Native.blunder_gameplay_input_was_jump_pressed(&pressed) != Native.Ok)
        {
            throw new InvalidOperationException("gameplay_input_was_jump_pressed failed");
        }
        return pressed != 0;
    }
}
