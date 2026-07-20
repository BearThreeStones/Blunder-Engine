using System.Runtime.InteropServices;

namespace Blunder;

/// <summary>P/Invoke surface for SHARED <c>blunder_engine_c</c> (C-ABI v2).</summary>
internal static class Native
{
    public const string Lib = "blunder_engine_c";

    public const int Ok = 0;
    public const int Error = 1;

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int blunder_engine_abi_version();

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern ulong blunder_object_create();

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int blunder_object_destroy(ulong id);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int blunder_object_is_valid(ulong id);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int blunder_object_set_bool_property(
        ulong id,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string className,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string propertyName,
        int value);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int blunder_object_get_bool_property(
        ulong id,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string className,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string propertyName,
        out int outValue);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern ulong blunder_object_add_behaviour(
        ulong id,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string typeName);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int blunder_object_remove_behaviour(ulong id, ulong behaviourId);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int blunder_object_behaviour_count(ulong id);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern ulong blunder_object_behaviour_id_at(ulong id, int index);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int blunder_object_set_behaviour_peer(
        ulong id, ulong behaviourId, IntPtr peer);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr blunder_object_get_behaviour_peer(ulong id, ulong behaviourId);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int blunder_object_set_vec3_property(
        ulong id,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string className,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string propertyName,
        float x,
        float y,
        float z);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int blunder_object_get_vec3_property(
        ulong id,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string className,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string propertyName,
        out float x,
        out float y,
        out float z);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern int blunder_lifecycle_clear_hooks();
}
