namespace DotnetHostGame;

/// <summary>Fixture: Tick throws to exercise Lifecycle exception catch.</summary>
public sealed class ThrowingTickBehaviour : Blunder.Behaviour
{
    public override void Tick(float deltaTime)
    {
        throw new System.InvalidOperationException("throwing tick");
    }
}
