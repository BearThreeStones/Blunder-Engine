using System.Runtime.InteropServices;
using System.Text;

namespace Blunder;

/// <summary>
/// Managed façade for a native Sync Group (C-ABI v7).
/// Members are co-located AnimationPlayers on Objects.
/// </summary>
public sealed class AnimationSyncGroup
{
    readonly ulong _groupId;

    AnimationSyncGroup(ulong groupId) => _groupId = groupId;

    public ulong GroupId => _groupId;

    public static AnimationSyncGroup Create()
    {
        ulong groupId = Native.blunder_sync_group_create();
        if (groupId == 0)
        {
            throw new InvalidOperationException("blunder_sync_group_create failed.");
        }

        return new AnimationSyncGroup(groupId);
    }

    public bool Destroy() => Native.blunder_sync_group_destroy(_groupId) == Native.Ok;

    public bool Join(ObjectHandle player) =>
        Native.blunder_sync_group_join(_groupId, player.Id) == Native.Ok;

    public bool Leave(ObjectHandle player) =>
        Native.blunder_sync_group_leave(_groupId, player.Id) == Native.Ok;

    public unsafe bool Fire(ReadOnlySpan<SyncGroupFireInstruction> instructions)
    {
        if (instructions.IsEmpty)
        {
            return Native.blunder_sync_group_fire(_groupId, null, 0) == Native.Ok;
        }

        int count = instructions.Length;
        byte[][] clipUtf8 = new byte[count][];
        GCHandle[] pins = new GCHandle[count];
        BlunderSyncGroupFireInstruction[] native = new BlunderSyncGroupFireInstruction[count];

        try
        {
            for (int i = 0; i < count; ++i)
            {
                clipUtf8[i] = ToUtf8(instructions[i].ClipName);
                pins[i] = GCHandle.Alloc(clipUtf8[i], GCHandleType.Pinned);
                native[i].player_object_id = instructions[i].Player.Id;
                native[i].clip_name = (byte*)pins[i].AddrOfPinnedObject();
                if (instructions[i].SeekSeconds is float seekSeconds)
                {
                    native[i].has_seek = 1;
                    native[i].seek_seconds = seekSeconds;
                }
                else
                {
                    native[i].has_seek = 0;
                    native[i].seek_seconds = 0f;
                }
            }

            fixed (BlunderSyncGroupFireInstruction* nativePtr = native)
            {
                return Native.blunder_sync_group_fire(_groupId, nativePtr, count) ==
                       Native.Ok;
            }
        }
        finally
        {
            for (int i = 0; i < count; ++i)
            {
                if (pins[i].IsAllocated)
                {
                    pins[i].Free();
                }
            }
        }
    }

    public bool FireSameName(string clipName) =>
        Native.blunder_sync_group_fire_same_name(_groupId, clipName) == Native.Ok;

    public bool FireSameName(string clipName, float seekSeconds) =>
        Native.blunder_sync_group_fire_same_name_seek(_groupId, clipName, seekSeconds) ==
        Native.Ok;

    static byte[] ToUtf8(string value)
    {
        if (string.IsNullOrEmpty(value))
        {
            return [0];
        }

        int byteCount = Encoding.UTF8.GetByteCount(value);
        byte[] buffer = new byte[byteCount + 1];
        Encoding.UTF8.GetBytes(value, buffer.AsSpan(0, byteCount));
        return buffer;
    }
}

/// <summary>Per-member Fire instruction for <see cref="AnimationSyncGroup.Fire"/>.</summary>
public readonly struct SyncGroupFireInstruction
{
    public ObjectHandle Player { get; init; }
    public string ClipName { get; init; }
    public float? SeekSeconds { get; init; }
}
