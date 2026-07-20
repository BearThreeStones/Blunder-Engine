using System.Runtime.InteropServices;

namespace Blunder.ScriptHost;

/// <summary>
/// Maps native <c>BehaviourId</c> values to managed <see cref="GCHandle"/> peers.
/// </summary>
internal static class PeerTable
{
    static readonly Dictionary<ulong, GCHandle> Handles = new();

    public static void Register(ulong behaviourId, GCHandle handle)
    {
        Handles[behaviourId] = handle;
    }

    public static bool TryGet(ulong behaviourId, out GCHandle handle) =>
        Handles.TryGetValue(behaviourId, out handle);

    public static void Remove(ulong behaviourId)
    {
        if (Handles.Remove(behaviourId, out GCHandle handle) && handle.IsAllocated)
        {
            handle.Free();
        }
    }

    public static void Clear()
    {
        foreach (GCHandle handle in Handles.Values)
        {
            if (handle.IsAllocated)
            {
                handle.Free();
            }
        }

        Handles.Clear();
    }
}
