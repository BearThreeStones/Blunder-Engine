namespace Blunder;

/// <summary>
/// Managed façade for an Object's Character Controller Unique (C-ABI v13).
/// <see cref="MoveAndSlide"/> sweeps; <see cref="ObjectHandle.Position"/> writes teleport.
/// </summary>
public sealed class CharacterController
{
    readonly ObjectHandle _owner;

    internal CharacterController(ObjectHandle owner) => _owner = owner;

    public Vec3 Velocity
    {
        get
        {
            if (Native.blunder_character_controller_get_velocity(
                    _owner.Id, out float x, out float y, out float z) != Native.Ok)
            {
                return default;
            }

            return new Vec3(x, y, z);
        }
        set => Native.blunder_character_controller_set_velocity(
            _owner.Id, value.X, value.Y, value.Z);
    }

    public bool IsOnFloor
    {
        get
        {
            if (Native.blunder_character_controller_is_on_floor(_owner.Id, out int value) !=
                Native.Ok)
            {
                return false;
            }

            return value != 0;
        }
    }

    public bool IsOnWall
    {
        get
        {
            if (Native.blunder_character_controller_is_on_wall(_owner.Id, out int value) !=
                Native.Ok)
            {
                return false;
            }

            return value != 0;
        }
    }

    public bool IsOnCeiling
    {
        get
        {
            if (Native.blunder_character_controller_is_on_ceiling(_owner.Id, out int value) !=
                Native.Ok)
            {
                return false;
            }

            return value != 0;
        }
    }

    public bool MoveAndSlide() =>
        Native.blunder_character_controller_move_and_slide(_owner.Id) == Native.Ok;
}
