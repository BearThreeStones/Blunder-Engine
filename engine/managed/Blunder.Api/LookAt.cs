namespace Blunder;

/// <summary>
/// Managed façade for a SkeletonLookAtModifier at <see cref="Index"/> (C-ABI v10).
/// </summary>
public sealed class LookAt
{
    readonly ObjectHandle _owner;

    internal LookAt(ObjectHandle owner, int index)
    {
        _owner = owner;
        Index = index;
    }

    public int Index { get; }

    public Vec3 Target
    {
        get
        {
            if (Native.blunder_skeleton_modifier_get_look_at_target(
                    _owner.Id, Index, out float x, out float y, out float z) != Native.Ok)
            {
                return default;
            }

            return new Vec3(x, y, z);
        }
        set =>
            Native.blunder_skeleton_modifier_set_look_at_target(
                _owner.Id, Index, value.X, value.Y, value.Z);
    }

    public string BoneName
    {
        get
        {
            if (Native.blunder_skeleton_modifier_get_look_at_bone_name(
                    _owner.Id, Index, out string boneName) != Native.Ok)
            {
                return "";
            }

            return boneName;
        }
        set =>
            Native.blunder_skeleton_modifier_set_look_at_bone_name(
                _owner.Id, Index, value ?? "");
    }
}
