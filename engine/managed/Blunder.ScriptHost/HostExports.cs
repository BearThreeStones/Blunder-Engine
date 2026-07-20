using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;

namespace Blunder.ScriptHost;

/// <summary>
/// UnmanagedCallersOnly entry points resolved by native <c>DotNetHost</c> via hostfxr.
/// </summary>
public static class HostExports
{
    static Assembly? s_gameAssembly;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static int LoadGameAssembly(IntPtr utf8Path)
    {
        try
        {
            string? path = Utf8ToString(utf8Path);
            if (string.IsNullOrEmpty(path) || !File.Exists(path))
            {
                return Native.Error;
            }

            s_gameAssembly = Assembly.LoadFrom(path);
            return Native.Ok;
        }
        catch
        {
            return Native.Error;
        }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int AttachBehaviour(
        ulong objectId, IntPtr utf8TypeName, ulong* behaviourId)
    {
        if (behaviourId == null)
        {
            return Native.Error;
        }

        *behaviourId = 0;

        try
        {
            string? typeName = Utf8ToString(utf8TypeName);
            if (string.IsNullOrEmpty(typeName) || objectId == 0)
            {
                return Native.Error;
            }

            Type? type = ResolveBehaviourType(typeName);
            if (type == null || !typeof(Behaviour).IsAssignableFrom(type) ||
                type.IsAbstract)
            {
                return Native.Error;
            }

            ulong id = Native.blunder_object_add_behaviour(objectId, typeName);
            if (id == 0)
            {
                return Native.Error;
            }

            object? instance = Activator.CreateInstance(type);
            if (instance is not Behaviour behaviour)
            {
                Native.blunder_object_remove_behaviour(objectId, id);
                return Native.Error;
            }

            behaviour.Object = new ObjectHandle(objectId);
            behaviour.BehaviourId = id;

            GCHandle handle = GCHandle.Alloc(behaviour, GCHandleType.Normal);
            IntPtr peer = GCHandle.ToIntPtr(handle);
            if (Native.blunder_object_set_behaviour_peer(objectId, id, peer) !=
                Native.Ok)
            {
                handle.Free();
                Native.blunder_object_remove_behaviour(objectId, id);
                return Native.Error;
            }

            PeerTable.Register(id, handle);
            *behaviourId = id;
            return Native.Ok;
        }
        catch
        {
            return Native.Error;
        }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static void RegisterLifecycleHooks()
    {
        unsafe
        {
            delegate* unmanaged[Cdecl]<IntPtr, float, void> tick = &OnTick;
            delegate* unmanaged[Cdecl]<IntPtr, void> ready = &OnReady;
            Native.blunder_lifecycle_set_tick_hook("Object", (IntPtr)tick);
            Native.blunder_lifecycle_set_ready_hook("Object", (IntPtr)ready);
        }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static void OnTick(IntPtr peer, float deltaTime)
    {
        if (peer == IntPtr.Zero)
        {
            return;
        }

        GCHandle handle = GCHandle.FromIntPtr(peer);
        if (handle.Target is Behaviour behaviour)
        {
            behaviour.Tick(deltaTime);
        }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static void OnReady(IntPtr peer)
    {
        if (peer == IntPtr.Zero)
        {
            return;
        }

        GCHandle handle = GCHandle.FromIntPtr(peer);
        if (handle.Target is Behaviour behaviour)
        {
            behaviour.Ready();
        }
    }

    static Type? ResolveBehaviourType(string typeName)
    {
        if (s_gameAssembly != null)
        {
            Type? fromGame = s_gameAssembly.GetType(typeName, throwOnError: false);
            if (fromGame != null)
            {
                return fromGame;
            }

            foreach (Type candidate in s_gameAssembly.GetTypes())
            {
                if (candidate.FullName == typeName || candidate.Name == typeName)
                {
                    return candidate;
                }
            }
        }

        return Type.GetType(typeName, throwOnError: false);
    }

    static string? Utf8ToString(IntPtr utf8)
    {
        if (utf8 == IntPtr.Zero)
        {
            return null;
        }

        unsafe
        {
            byte* p = (byte*)utf8;
            int len = 0;
            while (p[len] != 0)
            {
                ++len;
            }

            return Encoding.UTF8.GetString(p, len);
        }
    }
}
