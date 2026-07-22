namespace Blunder;

public readonly struct MessageId
{
    public uint Value { get; }

    internal MessageId(uint value) => Value = value;
}

public static class Message
{
    public static MessageId Register(string name)
    {
        unsafe
        {
            uint id = 0;
            int rc = Native.blunder_message_register(name, &id);
            if (rc != Native.Ok)
            {
                throw new InvalidOperationException(
                    $"blunder_message_register failed for '{name}' (rc={rc}).");
            }

            return new MessageId(id);
        }
    }

    public static void Send(ulong objectId, MessageId id, params MessageArg[]? args)
    {
        args ??= [];
        if (args.Length > 4)
        {
            throw new ArgumentException(
                "Message.Send supports at most 4 arguments.", nameof(args));
        }

        unsafe
        {
            if (args.Length == 0)
            {
                int rc = Native.blunder_message_send(objectId, id.Value, null, 0);
                if (rc != Native.Ok)
                {
                    throw new InvalidOperationException(
                        $"blunder_message_send failed (rc={rc}).");
                }

                return;
            }

            BlunderMessageArg* buffer = stackalloc BlunderMessageArg[args.Length];
            for (int i = 0; i < args.Length; ++i)
            {
                buffer[i] = args[i].ToNative();
            }

            int sendRc = Native.blunder_message_send(
                objectId, id.Value, buffer, args.Length);
            if (sendRc != Native.Ok)
            {
                throw new InvalidOperationException(
                    $"blunder_message_send failed (rc={sendRc}).");
            }
        }
    }
}
