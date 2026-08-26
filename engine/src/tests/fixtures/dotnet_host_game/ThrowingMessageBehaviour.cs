namespace DotnetHostGame;

/// <summary>Fixture: OnMessage throws to exercise sibling Message delivery.</summary>
public sealed class ThrowingMessageBehaviour : Blunder.Behaviour
{
    public override void OnMessage(Blunder.MessageId id, ReadOnlySpan<Blunder.MessageArg> args)
    {
        throw new System.InvalidOperationException("throwing message");
    }
}
