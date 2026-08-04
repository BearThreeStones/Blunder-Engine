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
        abi.lifecycle_clear_hooks != null &&
        abi.gameplay_input_get_move != null &&
        abi.gameplay_input_was_jump_pressed != null &&
        abi.message_register != null &&
        abi.message_send != null &&
        abi.message_set_hook != null &&
        abi.message_clear_hook != null &&
        abi.animation_player_play != null &&
        abi.animation_player_play_with_fade != null &&
        abi.animation_player_stop != null &&
        abi.animation_player_set_slot != null &&
        abi.animation_player_get_slot != null &&
        abi.animation_player_set_blend_weight != null &&
        abi.animation_player_get_blend_weight != null &&
        abi.animation_player_set_time_scale != null &&
        abi.animation_player_get_time_scale != null &&
        abi.animation_player_set_loop != null &&
        abi.animation_player_get_playback_position != null &&
        abi.animation_player_get_clip_length != null &&
        abi.animation_player_add_pose_applied_listener != null &&
        abi.animation_player_clear_pose_applied_listeners != null &&
        abi.animation_tree_set_active != null &&
        abi.animation_tree_get_active != null &&
        abi.animation_tree_travel != null &&
        abi.animation_tree_start != null &&
        abi.animation_tree_set_blend_space_scalar != null &&
        abi.animation_tree_get_blend_space_scalar != null &&
        abi.animation_tree_request_one_shot != null &&
        abi.animation_tree_set_add2_weight != null &&
        abi.animation_tree_get_add2_weight != null &&
        abi.animation_tree_set_blend_space_2d_param != null &&
        abi.animation_tree_get_blend_space_2d_param != null &&
        abi.animation_tree_set_asset_guid != null &&
        abi.animation_tree_get_asset_guid != null &&
        abi.skeleton_modifier_count != null &&
        abi.skeleton_modifier_set_enabled != null &&
        abi.skeleton_modifier_get_enabled != null &&
        abi.skeleton_modifier_move != null &&
        abi.animation_player_get_method_key_count != null &&
        abi.animation_player_get_method_key != null &&
        abi.sync_group_create != null &&
        abi.sync_group_destroy != null &&
        abi.sync_group_join != null &&
        abi.sync_group_leave != null &&
        abi.sync_group_fire != null &&
        abi.sync_group_fire_same_name != null &&
        abi.sync_group_fire_same_name_seek != null &&
        abi.cine_enter != null &&
        abi.cine_end != null &&
        abi.cine_is_in_cine != null &&
        abi.cine_is_gameplay_input_suppressed != null;

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

    public static int blunder_gameplay_input_get_move(float* outX, float* outY)
    {
        EnsureRegistered();
        return s_abi.gameplay_input_get_move(outX, outY);
    }

    public static int blunder_gameplay_input_was_jump_pressed(int* outPressed)
    {
        EnsureRegistered();
        return s_abi.gameplay_input_was_jump_pressed(outPressed);
    }

    public static int blunder_message_register(string name, uint* outId)
    {
        EnsureRegistered();
        byte[] nameUtf8 = ToUtf8(name);
        fixed (byte* namePtr = nameUtf8)
        {
            return s_abi.message_register(namePtr, outId);
        }
    }

    public static int blunder_message_send(
        ulong target, uint id, BlunderMessageArg* args, int argc)
    {
        EnsureRegistered();
        return s_abi.message_send(target, id, args, argc);
    }

    public static int blunder_message_set_hook(
        delegate* unmanaged[Cdecl]<void*, uint, BlunderMessageArg*, int, void> hook)
    {
        EnsureRegistered();
        return s_abi.message_set_hook(hook);
    }

    public static int blunder_message_clear_hook()
    {
        EnsureRegistered();
        return s_abi.message_clear_hook();
    }

    public static int blunder_animation_player_play(ulong id, string clipName)
    {
        EnsureRegistered();
        byte[] clipUtf8 = ToUtf8(clipName);
        fixed (byte* clipPtr = clipUtf8)
        {
            return s_abi.animation_player_play(id, clipPtr);
        }
    }

    public static int blunder_animation_player_play_with_fade(
        ulong id, string clipName, float fadeSeconds)
    {
        EnsureRegistered();
        byte[] clipUtf8 = ToUtf8(clipName);
        fixed (byte* clipPtr = clipUtf8)
        {
            return s_abi.animation_player_play_with_fade(id, clipPtr, fadeSeconds);
        }
    }

    public static int blunder_animation_player_stop(ulong id)
    {
        EnsureRegistered();
        return s_abi.animation_player_stop(id);
    }

    public static int blunder_animation_player_set_slot(
        ulong id, int slotIndex, string clipName)
    {
        EnsureRegistered();
        byte[] clipUtf8 = ToUtf8(clipName);
        fixed (byte* clipPtr = clipUtf8)
        {
            return s_abi.animation_player_set_slot(id, slotIndex, clipPtr);
        }
    }

    public static int blunder_animation_player_get_slot(
        ulong id, int slotIndex, out string clipName)
    {
        EnsureRegistered();
        clipName = "";
        const int capacity = 256;
        byte[] buffer = new byte[capacity];
        fixed (byte* namePtr = buffer)
        {
            int rc = s_abi.animation_player_get_slot(id, slotIndex, namePtr, capacity);
            if (rc != Ok)
            {
                return rc;
            }

            int length = 0;
            while (length < capacity - 1 && buffer[length] != 0)
            {
                ++length;
            }

            clipName = length == 0 ? "" : Encoding.UTF8.GetString(buffer, 0, length);
            return rc;
        }
    }

    public static int blunder_animation_player_set_blend_weight(ulong id, float weight)
    {
        EnsureRegistered();
        return s_abi.animation_player_set_blend_weight(id, weight);
    }

    public static int blunder_animation_player_get_blend_weight(ulong id, out float weight)
    {
        EnsureRegistered();
        weight = 0;
        float value = 0;
        int rc = s_abi.animation_player_get_blend_weight(id, &value);
        weight = value;
        return rc;
    }

    public static int blunder_animation_player_set_time_scale(ulong id, float scale)
    {
        EnsureRegistered();
        return s_abi.animation_player_set_time_scale(id, scale);
    }

    public static int blunder_animation_player_get_time_scale(ulong id, out float scale)
    {
        EnsureRegistered();
        scale = 0;
        float value = 0;
        int rc = s_abi.animation_player_get_time_scale(id, &value);
        scale = value;
        return rc;
    }

    public static int blunder_animation_player_set_loop(ulong id, int loop)
    {
        EnsureRegistered();
        return s_abi.animation_player_set_loop(id, loop);
    }

    public static int blunder_animation_player_get_playback_position(
        ulong id, out float position)
    {
        EnsureRegistered();
        position = 0;
        float value = 0;
        int rc = s_abi.animation_player_get_playback_position(id, &value);
        position = value;
        return rc;
    }

    public static int blunder_animation_player_get_clip_length(ulong id, out float length)
    {
        EnsureRegistered();
        length = 0;
        float value = 0;
        int rc = s_abi.animation_player_get_clip_length(id, &value);
        length = value;
        return rc;
    }

    public static int blunder_animation_player_add_pose_applied_listener(
        ulong id,
        delegate* unmanaged[Cdecl]<ulong, void*, void> hook,
        void* userdata)
    {
        EnsureRegistered();
        return s_abi.animation_player_add_pose_applied_listener(id, hook, userdata);
    }

    public static int blunder_animation_player_clear_pose_applied_listeners(ulong id)
    {
        EnsureRegistered();
        return s_abi.animation_player_clear_pose_applied_listeners(id);
    }

    public static int blunder_animation_tree_set_active(ulong id, int active)
    {
        EnsureRegistered();
        return s_abi.animation_tree_set_active(id, active);
    }

    public static int blunder_animation_tree_get_active(ulong id, out int active)
    {
        EnsureRegistered();
        active = 0;
        int value = 0;
        int rc = s_abi.animation_tree_get_active(id, &value);
        active = value;
        return rc;
    }

    public static int blunder_animation_tree_travel(ulong id, string stateName)
    {
        EnsureRegistered();
        byte[] utf8 = ToUtf8(stateName);
        fixed (byte* namePtr = utf8)
        {
            return s_abi.animation_tree_travel(id, namePtr);
        }
    }

    public static int blunder_animation_tree_start(ulong id, string stateName)
    {
        EnsureRegistered();
        byte[] utf8 = ToUtf8(stateName);
        fixed (byte* namePtr = utf8)
        {
            return s_abi.animation_tree_start(id, namePtr);
        }
    }

    public static int blunder_animation_tree_set_blend_space_scalar(
        ulong id, string nodeName, float scalar)
    {
        EnsureRegistered();
        byte[] utf8 = ToUtf8(nodeName);
        fixed (byte* namePtr = utf8)
        {
            return s_abi.animation_tree_set_blend_space_scalar(id, namePtr, scalar);
        }
    }

    public static int blunder_animation_tree_get_blend_space_scalar(
        ulong id, string nodeName, out float scalar)
    {
        EnsureRegistered();
        scalar = 0;
        byte[] utf8 = ToUtf8(nodeName);
        fixed (byte* namePtr = utf8)
        {
            float value = 0;
            int rc = s_abi.animation_tree_get_blend_space_scalar(id, namePtr, &value);
            scalar = value;
            return rc;
        }
    }

    public static int blunder_animation_tree_request_one_shot(ulong id, string clipName)
    {
        EnsureRegistered();
        byte[] utf8 = ToUtf8(clipName);
        fixed (byte* namePtr = utf8)
        {
            return s_abi.animation_tree_request_one_shot(id, namePtr);
        }
    }

    public static int blunder_animation_tree_set_add2_weight(ulong id, float weight)
    {
        EnsureRegistered();
        return s_abi.animation_tree_set_add2_weight(id, weight);
    }

    public static int blunder_animation_tree_get_add2_weight(ulong id, out float weight)
    {
        EnsureRegistered();
        weight = 0;
        float value = 0;
        int rc = s_abi.animation_tree_get_add2_weight(id, &value);
        weight = value;
        return rc;
    }

    public static int blunder_animation_tree_set_blend_space_2d_param(
        ulong id, string nodeName, float x, float y)
    {
        EnsureRegistered();
        byte[] utf8 = ToUtf8(nodeName);
        fixed (byte* namePtr = utf8)
        {
            return s_abi.animation_tree_set_blend_space_2d_param(id, namePtr, x, y);
        }
    }

    public static int blunder_animation_tree_get_blend_space_2d_param(
        ulong id, string nodeName, out float x, out float y)
    {
        EnsureRegistered();
        x = 0;
        y = 0;
        byte[] utf8 = ToUtf8(nodeName);
        fixed (byte* namePtr = utf8)
        {
            float ox = 0;
            float oy = 0;
            int rc = s_abi.animation_tree_get_blend_space_2d_param(id, namePtr, &ox, &oy);
            x = ox;
            y = oy;
            return rc;
        }
    }

    public static int blunder_animation_tree_set_asset_guid(ulong id, string guid)
    {
        EnsureRegistered();
        byte[] utf8 = ToUtf8(guid ?? "");
        fixed (byte* guidPtr = utf8)
        {
            return s_abi.animation_tree_set_asset_guid(id, guidPtr);
        }
    }

    public static int blunder_animation_tree_get_asset_guid(ulong id, out string guid)
    {
        EnsureRegistered();
        guid = "";
        const int capacity = 128;
        byte[] buffer = new byte[capacity];
        fixed (byte* guidPtr = buffer)
        {
            int rc = s_abi.animation_tree_get_asset_guid(id, guidPtr, capacity);
            if (rc == Ok)
            {
                int len = 0;
                while (len < capacity && buffer[len] != 0)
                {
                    ++len;
                }

                guid = Encoding.UTF8.GetString(buffer, 0, len);
            }

            return rc;
        }
    }

    public static int blunder_skeleton_modifier_count(ulong id, out int count)
    {
        EnsureRegistered();
        count = 0;
        int value = 0;
        int rc = s_abi.skeleton_modifier_count(id, &value);
        count = value;
        return rc;
    }

    public static int blunder_skeleton_modifier_set_enabled(ulong id, int index, int enabled)
    {
        EnsureRegistered();
        return s_abi.skeleton_modifier_set_enabled(id, index, enabled);
    }

    public static int blunder_skeleton_modifier_get_enabled(ulong id, int index, out int enabled)
    {
        EnsureRegistered();
        enabled = 0;
        int value = 0;
        int rc = s_abi.skeleton_modifier_get_enabled(id, index, &value);
        enabled = value;
        return rc;
    }

    public static int blunder_skeleton_modifier_move(ulong id, int fromIndex, int toIndex)
    {
        EnsureRegistered();
        return s_abi.skeleton_modifier_move(id, fromIndex, toIndex);
    }

    public static int blunder_animation_player_get_method_key_count(
        ulong id, string clipName, out int count)
    {
        EnsureRegistered();
        count = 0;
        byte[] utf8 = ToUtf8(clipName);
        fixed (byte* namePtr = utf8)
        {
            int value = 0;
            int rc = s_abi.animation_player_get_method_key_count(id, namePtr, &value);
            count = value;
            return rc;
        }
    }

    public static int blunder_animation_player_get_method_key(
        ulong id, string clipName, int index, out string name, out float time)
    {
        EnsureRegistered();
        name = "";
        time = 0;
        const int capacity = 256;
        byte[] buffer = new byte[capacity];
        byte[] utf8 = ToUtf8(clipName);
        fixed (byte* clipPtr = utf8)
        fixed (byte* namePtr = buffer)
        {
            float value = 0;
            int rc = s_abi.animation_player_get_method_key(
                id, clipPtr, index, namePtr, capacity, &value);
            if (rc == Ok)
            {
                int len = 0;
                while (len < capacity && buffer[len] != 0)
                {
                    ++len;
                }

                name = Encoding.UTF8.GetString(buffer, 0, len);
                time = value;
            }

            return rc;
        }
    }

    public static ulong blunder_sync_group_create()
    {
        EnsureRegistered();
        return s_abi.sync_group_create();
    }

    public static int blunder_sync_group_destroy(ulong id)
    {
        EnsureRegistered();
        return s_abi.sync_group_destroy(id);
    }

    public static int blunder_sync_group_join(ulong groupId, ulong playerObjectId)
    {
        EnsureRegistered();
        return s_abi.sync_group_join(groupId, playerObjectId);
    }

    public static int blunder_sync_group_leave(ulong groupId, ulong playerObjectId)
    {
        EnsureRegistered();
        return s_abi.sync_group_leave(groupId, playerObjectId);
    }

    public static int blunder_sync_group_fire(
        ulong groupId, BlunderSyncGroupFireInstruction* instructions, int instructionCount)
    {
        EnsureRegistered();
        return s_abi.sync_group_fire(groupId, instructions, instructionCount);
    }

    public static int blunder_sync_group_fire_same_name(ulong groupId, string clipName)
    {
        EnsureRegistered();
        byte[] clipUtf8 = ToUtf8(clipName);
        fixed (byte* clipPtr = clipUtf8)
        {
            return s_abi.sync_group_fire_same_name(groupId, clipPtr);
        }
    }

    public static int blunder_sync_group_fire_same_name_seek(
        ulong groupId, string clipName, float seekSeconds)
    {
        EnsureRegistered();
        byte[] clipUtf8 = ToUtf8(clipName);
        fixed (byte* clipPtr = clipUtf8)
        {
            return s_abi.sync_group_fire_same_name_seek(groupId, clipPtr, seekSeconds);
        }
    }

    public static int blunder_cine_enter(int suppressGameplayInput)
    {
        EnsureRegistered();
        return s_abi.cine_enter(suppressGameplayInput);
    }

    public static int blunder_cine_end()
    {
        EnsureRegistered();
        return s_abi.cine_end();
    }

    public static int blunder_cine_is_in_cine(out int inCine)
    {
        EnsureRegistered();
        inCine = 0;
        int value = 0;
        int rc = s_abi.cine_is_in_cine(&value);
        inCine = value;
        return rc;
    }

    public static int blunder_cine_is_gameplay_input_suppressed(out int suppressed)
    {
        EnsureRegistered();
        suppressed = 0;
        int value = 0;
        int rc = s_abi.cine_is_gameplay_input_suppressed(&value);
        suppressed = value;
        return rc;
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
