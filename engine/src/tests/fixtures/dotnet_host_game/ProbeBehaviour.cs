namespace DotnetHostGame;

/// <summary>Fixture Behaviour that counts Tick invocations for e2e smoke.</summary>
public sealed class ProbeBehaviour : Blunder.Behaviour
{
    public static int TickCount;

    /// <summary>Scene property-bag fields (bool / number / string MVP).</summary>
    public bool EnabledFlag;
    public float Speed;
    public string Label = "";

    public static bool LastEnabledFlag;
    public static float LastSpeed;
    public static string LastLabel = "";

    public override void Ready()
    {
        LastEnabledFlag = EnabledFlag;
        LastSpeed = Speed;
        LastLabel = Label ?? "";
    }

    public override void Tick(float deltaTime)
    {
        TickCount++;
    }
}
