namespace Blunder;

/// <summary>
/// Managed façade for an Object's co-located AnimationPlayer (C-ABI v5).
/// </summary>
public sealed class AnimationPlayer
{
    const string ClassName = "AnimationPlayer";

    readonly ObjectHandle _owner;

    internal AnimationPlayer(ObjectHandle owner) => _owner = owner;

    public bool Play(string clipName) =>
        Native.blunder_animation_player_play(_owner.Id, clipName) == Native.Ok;

    public void Stop() => Native.blunder_animation_player_stop(_owner.Id);

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
}
