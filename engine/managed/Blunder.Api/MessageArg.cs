using System.Runtime.InteropServices;

namespace Blunder;

public enum MessageArgKind : byte
{
    Nil = 0,
    Bool = 1,
    Int = 2,
    Float = 3,
    ObjectId = 4,
}

/// <summary>
/// Managed mirror of native <c>BlunderMessageArg</c> (C-ABI v4).
/// Layout: uint8 kind + 7-byte pad + 8-byte union (b/i/f/object_id).
/// </summary>
[StructLayout(LayoutKind.Explicit, Size = 16)]
public unsafe struct BlunderMessageArg
{
    [FieldOffset(0)] public byte kind;
    [FieldOffset(1)] byte _pad0;
    [FieldOffset(2)] byte _pad1;
    [FieldOffset(3)] byte _pad2;
    [FieldOffset(4)] byte _pad3;
    [FieldOffset(5)] byte _pad4;
    [FieldOffset(6)] byte _pad5;
    [FieldOffset(7)] byte _pad6;
    [FieldOffset(8)] public byte b;
    [FieldOffset(8)] public long i;
    [FieldOffset(8)] public float f;
    [FieldOffset(8)] public ulong object_id;
}

public struct MessageArg
{
    public MessageArgKind Kind { get; private set; }
    public long IntValue { get; private set; }
    public float FloatValue { get; private set; }
    public bool BoolValue { get; private set; }
    public ulong ObjectIdValue { get; private set; }

    public static MessageArg FromInt(long value) =>
        new() { Kind = MessageArgKind.Int, IntValue = value };

    public static MessageArg FromBool(bool value) =>
        new() { Kind = MessageArgKind.Bool, BoolValue = value };

    public static MessageArg FromFloat(float value) =>
        new() { Kind = MessageArgKind.Float, FloatValue = value };

    public static MessageArg FromObjectId(ulong value) =>
        new() { Kind = MessageArgKind.ObjectId, ObjectIdValue = value };

    internal BlunderMessageArg ToNative()
    {
        BlunderMessageArg native = default;
        native.kind = (byte)Kind;
        switch (Kind)
        {
            case MessageArgKind.Bool:
                native.b = (byte)(BoolValue ? 1 : 0);
                break;
            case MessageArgKind.Int:
                native.i = IntValue;
                break;
            case MessageArgKind.Float:
                native.f = FloatValue;
                break;
            case MessageArgKind.ObjectId:
                native.object_id = ObjectIdValue;
                break;
        }

        return native;
    }

    internal static MessageArg FromNative(in BlunderMessageArg native)
    {
        return (MessageArgKind)native.kind switch
        {
            MessageArgKind.Bool => FromBool(native.b != 0),
            MessageArgKind.Int => FromInt(native.i),
            MessageArgKind.Float => FromFloat(native.f),
            MessageArgKind.ObjectId => FromObjectId(native.object_id),
            _ => default,
        };
    }
}
