using System.Runtime.InteropServices;

namespace Blunder;

/// <summary>
/// Managed façade for an Object's co-located AnimationPlayer (C-ABI v9).
/// </summary>
public sealed class AnimationPlayer
{
    const string ClassName = "AnimationPlayer";

    readonly ObjectHandle _owner;
    GCHandle _selfHandle;
    bool _nativeListenerRegistered;
    event Action? _poseApplied;

    internal AnimationPlayer(ObjectHandle owner) => _owner = owner;

    /// <summary>
    /// Raised after skeleton sample each Play/advance (Tick → sample → PoseApplied).
    /// Content uses this plus <see cref="PlaybackPosition"/> frametime-modulo for Animation steps.
    /// </summary>
    public event Action PoseApplied
    {
        add
        {
            EnsureNativeListener();
            _poseApplied += value;
        }
        remove
        {
            _poseApplied -= value;
            if (_poseApplied == null)
            {
                ClearNativeListener();
            }
        }
    }

    public bool Play(string clipName) =>
        Native.blunder_animation_player_play(_owner.Id, clipName) == Native.Ok;

    public bool Play(string clipName, float fadeSeconds) =>
        Native.blunder_animation_player_play_with_fade(_owner.Id, clipName, fadeSeconds) ==
        Native.Ok;

    public void Stop() => Native.blunder_animation_player_stop(_owner.Id);

    public bool SetSlot(int slotIndex, string clipName) =>
        Native.blunder_animation_player_set_slot(_owner.Id, slotIndex, clipName) == Native.Ok;

    public string GetSlot(int slotIndex)
    {
        if (Native.blunder_animation_player_get_slot(_owner.Id, slotIndex, out string clipName) !=
            Native.Ok)
        {
            return "";
        }

        return clipName;
    }

    public float BlendWeight
    {
        get
        {
            if (Native.blunder_animation_player_get_blend_weight(_owner.Id, out float value) !=
                Native.Ok)
            {
                return 0f;
            }

            return value;
        }
        set => Native.blunder_animation_player_set_blend_weight(_owner.Id, value);
    }

    public float TimeScale
    {
        get
        {
            if (Native.blunder_animation_player_get_time_scale(_owner.Id, out float value) !=
                Native.Ok)
            {
                return 0f;
            }

            return value;
        }
        set => Native.blunder_animation_player_set_time_scale(_owner.Id, value);
    }

    public bool Loop
    {
        get
        {
            if (Native.blunder_object_get_bool_property(
                    _owner.Id, ClassName, "is_looping", out int value) != Native.Ok)
            {
                return false;
            }

            return value != 0;
        }
        set => Native.blunder_animation_player_set_loop(_owner.Id, value ? 1 : 0);
    }

    public bool IsPlaying
    {
        get
        {
            if (Native.blunder_object_get_bool_property(
                    _owner.Id, ClassName, "is_playing", out int value) != Native.Ok)
            {
                return false;
            }

            return value != 0;
        }
    }

    public float PlaybackPosition
    {
        get
        {
            if (Native.blunder_animation_player_get_playback_position(
                    _owner.Id, out float value) != Native.Ok)
            {
                return 0f;
            }

            return value;
        }
    }

    public float ClipLength
    {
        get
        {
            if (Native.blunder_animation_player_get_clip_length(
                    _owner.Id, out float value) != Native.Ok)
            {
                return 0f;
            }

            return value;
        }
    }

    public int GetMethodKeyCount(string clipName)
    {
        if (Native.blunder_animation_player_get_method_key_count(
                _owner.Id, clipName, out int count) != Native.Ok)
        {
            return 0;
        }

        return count;
    }

    public bool TryGetMethodKey(string clipName, int index, out string name, out float time)
    {
        if (Native.blunder_animation_player_get_method_key(
                _owner.Id, clipName, index, out name, out time) != Native.Ok)
        {
            name = "";
            time = 0f;
            return false;
        }

        return true;
    }

    internal void DetachNativeListeners()
    {
        _poseApplied = null;
        ClearNativeListener();
    }

    unsafe void EnsureNativeListener()
    {
        if (_nativeListenerRegistered)
        {
            return;
        }

        if (!_selfHandle.IsAllocated)
        {
            _selfHandle = GCHandle.Alloc(this);
        }

        int rc = Native.blunder_animation_player_add_pose_applied_listener(
            _owner.Id,
            &OnPoseAppliedNative,
            GCHandle.ToIntPtr(_selfHandle).ToPointer());
        if (rc != Native.Ok)
        {
            throw new InvalidOperationException(
                $"blunder_animation_player_add_pose_applied_listener failed (rc={rc}).");
        }

        _nativeListenerRegistered = true;
    }

    void ClearNativeListener()
    {
        if (_nativeListenerRegistered)
        {
            Native.blunder_animation_player_clear_pose_applied_listeners(_owner.Id);
            _nativeListenerRegistered = false;
        }

        if (_selfHandle.IsAllocated)
        {
            _selfHandle.Free();
        }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    static unsafe void OnPoseAppliedNative(ulong objectId, void* userdata)
    {
        if (userdata == null)
        {
            return;
        }

        GCHandle handle = GCHandle.FromIntPtr((IntPtr)userdata);
        if (handle.Target is not AnimationPlayer player)
        {
            return;
        }

        if (player._owner.Id != objectId)
        {
            return;
        }

        player._poseApplied?.Invoke();
    }
}
