namespace DotnetHostGame;

/// <summary>
/// Fixture Behaviour that counts OnMessage invocations for cross-Object
/// Message e2e smoke (object_message_dotnet_test).
/// </summary>
public sealed class MessageProbeBehaviour : Blunder.Behaviour
{
    public static int MessageCount;
    public static uint LastId;

    public override void OnMessage(Blunder.MessageId id, ReadOnlySpan<Blunder.MessageArg> args)
    {
        ++MessageCount;
        LastId = id.Value;
    }
}
