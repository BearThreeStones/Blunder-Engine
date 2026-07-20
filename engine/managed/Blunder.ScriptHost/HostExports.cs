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

    /// <summary>
    /// Loads game assemblies while forcing Blunder.Api to ScriptHost's copy.
    /// Default ALC probing can still materialize a second Api and break
    /// Behaviour type identity even with a Resolving hook.
    /// </summary>
    sealed class GameAssemblyLoadContext : System.Runtime.Loader.AssemblyLoadContext
    {
        public GameAssemblyLoadContext() : base(isCollectible: false) { }

        protected override Assembly? Load(AssemblyName assemblyName)
        {
            if (assemblyName.Name == "Blunder.Api")
            {
                return typeof(Behaviour).Assembly;
            }

            return null;
        }
    }

    static readonly GameAssemblyLoadContext s_gameAlc = new();

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

            string fullPath = Path.GetFullPath(path);
            s_gameAssembly = s_gameAlc.LoadFromAssemblyPath(fullPath);
            return Native.Ok;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"LoadGameAssembly exception: {ex}");
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
            if (type == null)
            {
                Console.Error.WriteLine(
                    $"AttachBehaviour: type not found: {typeName}");
                return Native.Error;
            }

            if (!typeof(Behaviour).IsAssignableFrom(type) || type.IsAbstract)
            {
                Console.Error.WriteLine(
                    $"AttachBehaviour: not a Behaviour: {type.FullName} " +
                    $"(base={type.BaseType?.AssemblyQualifiedName} " +
                    $"mvid={type.BaseType?.Assembly.ManifestModule.ModuleVersionId}; " +
                    $"expected={typeof(Behaviour).AssemblyQualifiedName} " +
                    $"mvid={typeof(Behaviour).Assembly.ManifestModule.ModuleVersionId})");
                return Native.Error;
            }

            ulong id = Native.blunder_object_add_behaviour(objectId, typeName);
            if (id == 0)
            {
                Console.Error.WriteLine(
                    $"AttachBehaviour: add_behaviour failed for object {objectId}");
                return Native.Error;
            }

            object? instance = Activator.CreateInstance(type);
            if (instance is not Behaviour behaviour)
            {
                Console.Error.WriteLine(
                    $"AttachBehaviour: CreateInstance type mismatch: {instance?.GetType().FullName}");
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
                Console.Error.WriteLine("AttachBehaviour: set_behaviour_peer failed");
                handle.Free();
                Native.blunder_object_remove_behaviour(objectId, id);
                return Native.Error;
            }

            PeerTable.Register(id, handle);
            *behaviourId = id;
            return Native.Ok;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"AttachBehaviour exception: {ex}");
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

    /// <summary>
    /// Test seam: read fixture <c>ProbeBehaviour.TickCount</c> from the game
    /// assembly already loaded by <see cref="LoadGameAssembly"/> (avoid a
    /// second hostfxr load that would duplicate statics).
    /// </summary>
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static int GetProbeTickCount()
    {
        try
        {
            if (s_gameAssembly == null)
            {
                return -1;
            }

            Type? type = s_gameAssembly.GetType("DotnetHostGame.ProbeBehaviour",
                                                throwOnError: false);
            if (type == null)
            {
                return -1;
            }

            FieldInfo? field = type.GetField(
                "TickCount", BindingFlags.Public | BindingFlags.Static);
            if (field == null || field.FieldType != typeof(int))
            {
                return -1;
            }

            object? value = field.GetValue(null);
            return value is int count ? count : -1;
        }
        catch
        {
            return -1;
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
