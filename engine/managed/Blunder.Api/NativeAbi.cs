using System.Runtime.InteropServices;

namespace Blunder;

/// <summary>
/// Managed mirror of native <c>BlunderNativeAbi</c> (C-ABI v2 function-pointer table).
/// Layout must match <c>engine_c_abi.h</c> field-for-field.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct BlunderNativeAbi
{
    public delegate* unmanaged[Cdecl]<int> engine_abi_version;
    public delegate* unmanaged[Cdecl]<ulong> object_create;
    public delegate* unmanaged[Cdecl]<ulong, int> object_destroy;
    public delegate* unmanaged[Cdecl]<ulong, int> object_is_valid;
    public delegate* unmanaged[Cdecl]<ulong, byte*, byte*, int, int> object_set_bool_property;
    public delegate* unmanaged[Cdecl]<ulong, byte*, byte*, int*, int> object_get_bool_property;
    public delegate* unmanaged[Cdecl]<ulong, byte*, ulong> object_add_behaviour;
    public delegate* unmanaged[Cdecl]<ulong, ulong, int> object_remove_behaviour;
    public delegate* unmanaged[Cdecl]<ulong, int> object_behaviour_count;
    public delegate* unmanaged[Cdecl]<ulong, int, ulong> object_behaviour_id_at;
    public delegate* unmanaged[Cdecl]<ulong, ulong, void*, int> object_set_behaviour_peer;
    public delegate* unmanaged[Cdecl]<ulong, ulong, void*> object_get_behaviour_peer;
    public delegate* unmanaged[Cdecl]<ulong, byte*, byte*, float, float, float, int>
        object_set_vec3_property;
    public delegate* unmanaged[Cdecl]<ulong, byte*, byte*, float*, float*, float*, int>
        object_get_vec3_property;
    public delegate* unmanaged[Cdecl]<byte*, void*, int> lifecycle_set_tick_hook;
    public delegate* unmanaged[Cdecl]<byte*, void*, int> lifecycle_set_ready_hook;
    public delegate* unmanaged[Cdecl]<int> lifecycle_clear_hooks;
}
