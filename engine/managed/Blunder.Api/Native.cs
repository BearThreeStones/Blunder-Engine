using System.Runtime.InteropServices;
using System.Text;

namespace Blunder;

/// <summary>
/// Managed C-ABI surface. Calls go through a native-registered
/// <see cref="BlunderNativeAbi"/> table — never process-default
/// <c>DllImport("blunder_engine_c")</c> (avoids a second ObjectDB image).
/// </summary>
internal static unsafe class Native
{
    public const int Ok = 0;
    public const int Error = 1;

    static BlunderNativeAbi s_abi;
    static bool s_registered;

    /// <summary>
    /// Stores a complete non-null C-ABI table for subsequent Native calls.
    /// </summary>
    internal static void Register(in BlunderNativeAbi abi)
    {
        if (!IsComplete(in abi))
        {
            throw new ArgumentException(
                "BlunderNativeAbi is incomplete: every entry point must be non-null.",
                nameof(abi));
        }

        s_abi = abi;
        s_registered = true;
    }

    /// <summary>Test seam: drop registration so "call before register" can be asserted.</summary>
    internal static void ClearRegistrationForTests()
    {
        s_abi = default;
        s_registered = false;
    }

    static bool IsComplete(in BlunderNativeAbi abi) =>
        abi.engine_abi_version != null &&
        abi.object_create != null &&
        abi.object_destroy != null &&
        abi.object_is_valid != null &&
        abi.object_set_bool_property != null &&
        abi.object_get_bool_property != null &&
        abi.object_add_behaviour != null &&
        abi.object_remove_behaviour != null &&
        abi.object_behaviour_count != null &&
        abi.object_behaviour_id_at != null &&
        abi.object_set_behaviour_peer != null &&
        abi.object_get_behaviour_peer != null &&
        abi.object_set_vec3_property != null &&
        abi.object_get_vec3_property != null &&
        abi.lifecycle_set_tick_hook != null &&
        abi.lifecycle_set_ready_hook != null &&
        abi.lifecycle_clear_hooks != null;

    static void EnsureRegistered()
    {
        if (!s_registered)
        {
            throw new InvalidOperationException(
                "Blunder.Api Native ABI is not registered. " +
                "DotNetHost must call ScriptHost RegisterNativeAbi before managed C-ABI use.");
        }
    }

    public static int blunder_engine_abi_version()
    {
        EnsureRegistered();
        return s_abi.engine_abi_version();
    }

    public static ulong blunder_object_create()
    {
        EnsureRegistered();
        return s_abi.object_create();
    }

    public static int blunder_object_destroy(ulong id)
    {
        EnsureRegistered();
        return s_abi.object_destroy(id);
    }

    public static int blunder_object_is_valid(ulong id)
    {
        EnsureRegistered();
        return s_abi.object_is_valid(id);
    }

    public static int blunder_object_set_bool_property(
        ulong id, string className, string propertyName, int value)
    {
        EnsureRegistered();
        byte[] classUtf8 = ToUtf8(className);
        byte[] propUtf8 = ToUtf8(propertyName);
        fixed (byte* classPtr = classUtf8)
        fixed (byte* propPtr = propUtf8)
        {
            return s_abi.object_set_bool_property(id, classPtr, propPtr, value);
        }
    }

    public static int blunder_object_get_bool_property(
        ulong id, string className, string propertyName, out int outValue)
    {
        EnsureRegistered();
        outValue = 0;
        byte[] classUtf8 = ToUtf8(className);
        byte[] propUtf8 = ToUtf8(propertyName);
        int value = 0;
        int rc;
        fixed (byte* classPtr = classUtf8)
        fixed (byte* propPtr = propUtf8)
        {
            rc = s_abi.object_get_bool_property(id, classPtr, propPtr, &value);
        }

        outValue = value;
        return rc;
    }

    public static ulong blunder_object_add_behaviour(ulong id, string typeName)
    {
        EnsureRegistered();
        byte[] typeUtf8 = ToUtf8(typeName);
        fixed (byte* typePtr = typeUtf8)
        {
            return s_abi.object_add_behaviour(id, typePtr);
        }
    }

    public static int blunder_object_remove_behaviour(ulong id, ulong behaviourId)
    {
        EnsureRegistered();
        return s_abi.object_remove_behaviour(id, behaviourId);
    }

    public static int blunder_object_behaviour_count(ulong id)
    {
        EnsureRegistered();
        return s_abi.object_behaviour_count(id);
    }

    public static ulong blunder_object_behaviour_id_at(ulong id, int index)
    {
        EnsureRegistered();
        return s_abi.object_behaviour_id_at(id, index);
    }

    public static int blunder_object_set_behaviour_peer(
        ulong id, ulong behaviourId, IntPtr peer)
    {
        EnsureRegistered();
        return s_abi.object_set_behaviour_peer(id, behaviourId, peer.ToPointer());
    }

    public static IntPtr blunder_object_get_behaviour_peer(ulong id, ulong behaviourId)
    {
        EnsureRegistered();
        return (IntPtr)s_abi.object_get_behaviour_peer(id, behaviourId);
    }

    public static int blunder_object_set_vec3_property(
        ulong id, string className, string propertyName, float x, float y, float z)
    {
        EnsureRegistered();
        byte[] classUtf8 = ToUtf8(className);
        byte[] propUtf8 = ToUtf8(propertyName);
        fixed (byte* classPtr = classUtf8)
        fixed (byte* propPtr = propUtf8)
        {
            return s_abi.object_set_vec3_property(id, classPtr, propPtr, x, y, z);
        }
    }

    public static int blunder_object_get_vec3_property(
        ulong id,
        string className,
        string propertyName,
        out float x,
        out float y,
        out float z)
    {
        EnsureRegistered();
        x = 0;
        y = 0;
        z = 0;
        byte[] classUtf8 = ToUtf8(className);
        byte[] propUtf8 = ToUtf8(propertyName);
        float ox = 0, oy = 0, oz = 0;
        int rc;
        fixed (byte* classPtr = classUtf8)
        fixed (byte* propPtr = propUtf8)
        {
            rc = s_abi.object_get_vec3_property(id, classPtr, propPtr, &ox, &oy, &oz);
        }

        x = ox;
        y = oy;
        z = oz;
        return rc;
    }

    public static int blunder_lifecycle_set_tick_hook(string className, IntPtr hook)
    {
        EnsureRegistered();
        byte[] classUtf8 = ToUtf8(className);
        fixed (byte* classPtr = classUtf8)
        {
            return s_abi.lifecycle_set_tick_hook(classPtr, hook.ToPointer());
        }
    }

    public static int blunder_lifecycle_set_ready_hook(string className, IntPtr hook)
    {
        EnsureRegistered();
        byte[] classUtf8 = ToUtf8(className);
        fixed (byte* classPtr = classUtf8)
        {
            return s_abi.lifecycle_set_ready_hook(classPtr, hook.ToPointer());
        }
    }

    public static int blunder_lifecycle_clear_hooks()
    {
        EnsureRegistered();
        return s_abi.lifecycle_clear_hooks();
    }

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
