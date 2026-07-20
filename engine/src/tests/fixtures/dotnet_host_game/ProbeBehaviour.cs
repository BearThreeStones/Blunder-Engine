namespace DotnetHostGame;

/// <summary>Fixture Behaviour that counts Tick invocations for e2e smoke.</summary>
public sealed class ProbeBehaviour : Blunder.Behaviour
{
    public static int TickCount;

    public override void Tick(float deltaTime)
    {
        TickCount++;
    }
}
