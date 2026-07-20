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
    /// Shared Api also means <see cref="RegisterNativeAbi"/> registration is
    /// visible to game assemblies (one Native static table).
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

    /// <summary>
    /// Stores the native C-ABI function-pointer table into Blunder.Api.
    /// Must run before any managed Native / ObjectHandle C-ABI call.
    /// </summary>
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int RegisterNativeAbi(BlunderNativeAbi* abi)
    {
        if (abi == null)
        {
            return Native.Error;
        }

        try
        {
            Native.Register(in *abi);
            return Native.Ok;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"RegisterNativeAbi exception: {ex}");
            return Native.Error;
        }
    }

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

        // In/out: non-zero = mount onto an existing restored BehaviourId slot.
        ulong preferredId = *behaviourId;
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

            ulong id;
            bool mountedExisting = false;
            if (preferredId != 0)
            {
                // Scene restore already created the slot; bind a peer only.
                IntPtr existingPeer =
                    Native.blunder_object_get_behaviour_peer(objectId, preferredId);
                if (existingPeer != IntPtr.Zero)
                {
                    *behaviourId = preferredId;
                    return Native.Ok;
                }

                id = preferredId;
                mountedExisting = true;
            }
            else
            {
                id = Native.blunder_object_add_behaviour(objectId, typeName);
                if (id == 0)
                {
                    Console.Error.WriteLine(
                        $"AttachBehaviour: add_behaviour failed for object {objectId}");
                    return Native.Error;
                }
            }

            object? instance = Activator.CreateInstance(type);
            if (instance is not Behaviour behaviour)
            {
                Console.Error.WriteLine(
                    $"AttachBehaviour: CreateInstance type mismatch: {instance?.GetType().FullName}");
                if (!mountedExisting)
                {
                    Native.blunder_object_remove_behaviour(objectId, id);
                }

                return Native.Error;
            }

            ObjectHandle host = ObjectHandle.GetOrCreate(objectId);
            behaviour.Object = host;
            behaviour.BehaviourId = id;
            host.RegisterBehaviour(behaviour);

            GCHandle handle = GCHandle.Alloc(behaviour, GCHandleType.Normal);
            IntPtr peer = GCHandle.ToIntPtr(handle);
            if (Native.blunder_object_set_behaviour_peer(objectId, id, peer) !=
                Native.Ok)
            {
                Console.Error.WriteLine("AttachBehaviour: set_behaviour_peer failed");
                handle.Free();
                if (!mountedExisting)
                {
                    Native.blunder_object_remove_behaviour(objectId, id);
                }

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

    /// <summary>
    /// Apply a JSON object of bool/number/string properties onto a mounted
    /// Behaviour. Unknown keys and type mismatches are skipped.
    /// </summary>
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int ApplyBehaviourProperties(
        ulong objectId, ulong behaviourId, IntPtr utf8Json)
    {
        if (objectId == 0 || behaviourId == 0)
        {
            return Native.Error;
        }

        try
        {
            string? json = Utf8ToString(utf8Json);
            if (string.IsNullOrEmpty(json))
            {
                return Native.Ok;
            }

            if (!PeerTable.TryGet(behaviourId, out GCHandle handle) ||
                handle.Target is not Behaviour behaviour)
            {
                Console.Error.WriteLine(
                    $"ApplyBehaviourProperties: no peer for behaviour {behaviourId}");
                return Native.Error;
            }

            ApplyJsonProperties(behaviour, json);
            return Native.Ok;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"ApplyBehaviourProperties exception: {ex}");
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
    public static int GetProbeTickCount() =>
        ReadGameStaticInt("DotnetHostGame.ProbeBehaviour", "TickCount");

    /// <summary>
    /// Test seam: read <c>SiblingProbeBehaviour.FoundSibling</c> after Ready
    /// (GetBehaviour sibling lookup via ScriptHost AttachBehaviour).
    /// </summary>
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static int GetProbeSiblingFound() =>
        ReadGameStaticInt("DotnetHostGame.SiblingProbeBehaviour", "FoundSibling");

    /// <summary>
    /// Test seam: 1 when Ready captured EnabledFlag/Speed/Label from the
    /// property bag (see ProbeBehaviour fixture).
    /// </summary>
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static int GetProbePropertyOk()
    {
        try
        {
            if (s_gameAssembly == null)
            {
                return -1;
            }

            Type? type = s_gameAssembly.GetType(
                "DotnetHostGame.ProbeBehaviour", throwOnError: false);
            if (type == null)
            {
                return -1;
            }

            bool flag = ReadStaticBool(type, "LastEnabledFlag");
            float speed = ReadStaticFloat(type, "LastSpeed");
            string label = ReadStaticString(type, "LastLabel");
            if (flag && Math.Abs(speed - 1.5f) < 0.001f && label == "hi")
            {
                return 1;
            }

            return 0;
        }
        catch
        {
            return -1;
        }
    }

    /// <summary>
    /// Clears managed peer table and type-level lifecycle hooks (host shutdown).
    /// </summary>
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static void ShutdownCleanup()
    {
        PeerTable.Clear();
        Native.blunder_lifecycle_clear_hooks();
        ObjectHandle.ClearRegistry();
    }

    static void ApplyJsonProperties(Behaviour behaviour, string json)
    {
        using System.Text.Json.JsonDocument doc =
            System.Text.Json.JsonDocument.Parse(json);
        Type type = behaviour.GetType();
        const BindingFlags flags =
            BindingFlags.Public | BindingFlags.Instance | BindingFlags.IgnoreCase;

        foreach (System.Text.Json.JsonProperty prop in doc.RootElement.EnumerateObject())
        {
            FieldInfo? field = type.GetField(prop.Name, flags);
            if (field != null)
            {
                if (TryConvertJson(prop.Value, field.FieldType, out object? value))
                {
                    field.SetValue(behaviour, value);
                }

                continue;
            }

            PropertyInfo? property = type.GetProperty(prop.Name, flags);
            if (property == null || !property.CanWrite ||
                property.GetIndexParameters().Length != 0)
            {
                continue;
            }

            if (TryConvertJson(prop.Value, property.PropertyType, out object? propValue))
            {
                property.SetValue(behaviour, propValue);
            }
        }
    }

    static bool TryConvertJson(
        System.Text.Json.JsonElement element, Type target, out object? value)
    {
        value = null;
        Type underlying = Nullable.GetUnderlyingType(target) ?? target;

        if (underlying == typeof(bool) &&
            (element.ValueKind == System.Text.Json.JsonValueKind.True ||
             element.ValueKind == System.Text.Json.JsonValueKind.False))
        {
            value = element.GetBoolean();
            return true;
        }

        if (underlying == typeof(string) &&
            element.ValueKind == System.Text.Json.JsonValueKind.String)
        {
            value = element.GetString() ?? "";
            return true;
        }

        if (element.ValueKind == System.Text.Json.JsonValueKind.Number)
        {
            if (underlying == typeof(float))
            {
                value = element.GetSingle();
                return true;
            }

            if (underlying == typeof(double))
            {
                value = element.GetDouble();
                return true;
            }

            if (underlying == typeof(int))
            {
                value = element.GetInt32();
                return true;
            }

            if (underlying == typeof(long))
            {
                value = element.GetInt64();
                return true;
            }
        }

        return false;
    }

    static int ReadGameStaticInt(string typeName, string fieldName)
    {
        try
        {
            if (s_gameAssembly == null)
            {
                return -1;
            }

            Type? type = s_gameAssembly.GetType(typeName, throwOnError: false);
            if (type == null)
            {
                return -1;
            }

            FieldInfo? field = type.GetField(
                fieldName, BindingFlags.Public | BindingFlags.Static);
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

    static bool ReadStaticBool(Type type, string fieldName)
    {
        FieldInfo? field = type.GetField(
            fieldName, BindingFlags.Public | BindingFlags.Static);
        return field?.FieldType == typeof(bool) && field.GetValue(null) is true;
    }

    static float ReadStaticFloat(Type type, string fieldName)
    {
        FieldInfo? field = type.GetField(
            fieldName, BindingFlags.Public | BindingFlags.Static);
        return field?.FieldType == typeof(float) && field.GetValue(null) is float f
            ? f
            : 0f;
    }

    static string ReadStaticString(Type type, string fieldName)
    {
        FieldInfo? field = type.GetField(
            fieldName, BindingFlags.Public | BindingFlags.Static);
        return field?.FieldType == typeof(string) && field.GetValue(null) is string s
            ? s
            : "";
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
