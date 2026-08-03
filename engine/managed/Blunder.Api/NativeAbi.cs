using System.Runtime.InteropServices;

namespace Blunder;

/// <summary>
/// Managed mirror of native <c>BlunderNativeAbi</c> (C-ABI v7 function-pointer table).
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
    public delegate* unmanaged[Cdecl]<float*, float*, int> gameplay_input_get_move;
    public delegate* unmanaged[Cdecl]<int*, int> gameplay_input_was_jump_pressed;
    public delegate* unmanaged[Cdecl]<byte*, uint*, int> message_register;
    public delegate* unmanaged[Cdecl]<ulong, uint, BlunderMessageArg*, int, int> message_send;
    public delegate* unmanaged[Cdecl]<delegate* unmanaged[Cdecl]<void*, uint, BlunderMessageArg*, int, void>, int>
        message_set_hook;
    public delegate* unmanaged[Cdecl]<int> message_clear_hook;
    public delegate* unmanaged[Cdecl]<ulong, byte*, int> animation_player_play;
    public delegate* unmanaged[Cdecl]<ulong, byte*, float, int> animation_player_play_with_fade;
    public delegate* unmanaged[Cdecl]<ulong, int> animation_player_stop;
    public delegate* unmanaged[Cdecl]<ulong, int, byte*, int> animation_player_set_slot;
    public delegate* unmanaged[Cdecl]<ulong, int, byte*, int, int> animation_player_get_slot;
    public delegate* unmanaged[Cdecl]<ulong, float, int> animation_player_set_blend_weight;
    public delegate* unmanaged[Cdecl]<ulong, float*, int> animation_player_get_blend_weight;
    public delegate* unmanaged[Cdecl]<ulong, float, int> animation_player_set_time_scale;
    public delegate* unmanaged[Cdecl]<ulong, float*, int> animation_player_get_time_scale;
    public delegate* unmanaged[Cdecl]<ulong, int, int> animation_player_set_loop;
    public delegate* unmanaged[Cdecl]<ulong, float*, int>
        animation_player_get_playback_position;
    public delegate* unmanaged[Cdecl]<ulong, float*, int>
        animation_player_get_clip_length;
    public delegate* unmanaged[Cdecl]<ulong, delegate* unmanaged[Cdecl]<ulong, void*, void>, void*, int>
        animation_player_add_pose_applied_listener;
    public delegate* unmanaged[Cdecl]<ulong, int>
        animation_player_clear_pose_applied_listeners;
    public delegate* unmanaged[Cdecl]<ulong> sync_group_create;
    public delegate* unmanaged[Cdecl]<ulong, int> sync_group_destroy;
    public delegate* unmanaged[Cdecl]<ulong, ulong, int> sync_group_join;
    public delegate* unmanaged[Cdecl]<ulong, ulong, int> sync_group_leave;
    public delegate* unmanaged[Cdecl]<ulong, BlunderSyncGroupFireInstruction*, int, int>
        sync_group_fire;
    public delegate* unmanaged[Cdecl]<ulong, byte*, int> sync_group_fire_same_name;
    public delegate* unmanaged[Cdecl]<ulong, byte*, float, int>
        sync_group_fire_same_name_seek;
    public delegate* unmanaged[Cdecl]<int, int> cine_enter;
    public delegate* unmanaged[Cdecl]<int> cine_end;
    public delegate* unmanaged[Cdecl]<int*, int> cine_is_in_cine;
    public delegate* unmanaged[Cdecl]<int*, int> cine_is_gameplay_input_suppressed;
}

/// <summary>
/// Managed mirror of native <c>BlunderSyncGroupFireInstruction</c>.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct BlunderSyncGroupFireInstruction
{
    public ulong player_object_id;
    public byte* clip_name;
    public float seek_seconds;
    public byte has_seek;
    public byte _padding0;
    public byte _padding1;
    public byte _padding2;
}
