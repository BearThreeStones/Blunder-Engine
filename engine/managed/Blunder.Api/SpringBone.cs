namespace Blunder;

/// <summary>
/// Managed façade for a SpringBone SkeletonModifier at <see cref="Index"/> (C-ABI v15).
/// </summary>
public sealed class SpringBone
{
    readonly ObjectHandle _owner;

    internal SpringBone(ObjectHandle owner, int index)
    {
        _owner = owner;
        Index = index;
    }

    public int Index { get; }

    public string RootBoneName
    {
        get
        {
            if (Native.blunder_skeleton_modifier_get_spring_bone_root_bone_name(
                    _owner.Id, Index, out string boneName) != Native.Ok)
            {
                return "";
            }

            return boneName;
        }
        set =>
            Native.blunder_skeleton_modifier_set_spring_bone_root_bone_name(
                _owner.Id, Index, value ?? "");
    }

    public string EndBoneName
    {
        get
        {
            if (Native.blunder_skeleton_modifier_get_spring_bone_end_bone_name(
                    _owner.Id, Index, out string boneName) != Native.Ok)
            {
                return "";
            }

            return boneName;
        }
        set =>
            Native.blunder_skeleton_modifier_set_spring_bone_end_bone_name(
                _owner.Id, Index, value ?? "");
    }

    public float Stiffness
    {
        get
        {
            if (Native.blunder_skeleton_modifier_get_spring_bone_stiffness(
                    _owner.Id, Index, out float stiffness) != Native.Ok)
            {
                return 0f;
            }

            return stiffness;
        }
        set =>
            Native.blunder_skeleton_modifier_set_spring_bone_stiffness(
                _owner.Id, Index, value);
    }

    public float Drag
    {
        get
        {
            if (Native.blunder_skeleton_modifier_get_spring_bone_drag(
                    _owner.Id, Index, out float drag) != Native.Ok)
            {
                return 0f;
            }

            return drag;
        }
        set =>
            Native.blunder_skeleton_modifier_set_spring_bone_drag(_owner.Id, Index, value);
    }

    public Vec3 Gravity
    {
        get
        {
            if (Native.blunder_skeleton_modifier_get_spring_bone_gravity(
                    _owner.Id, Index, out float x, out float y, out float z) != Native.Ok)
            {
                return default;
            }

            return new Vec3(x, y, z);
        }
        set =>
            Native.blunder_skeleton_modifier_set_spring_bone_gravity(
                _owner.Id, Index, value.X, value.Y, value.Z);
    }

    public float EndBoneLength
    {
        get
        {
            if (Native.blunder_skeleton_modifier_get_spring_bone_end_length(
                    _owner.Id, Index, out float length) != Native.Ok)
            {
                return 0f;
            }

            return length;
        }
        set =>
            Native.blunder_skeleton_modifier_set_spring_bone_end_length(
                _owner.Id, Index, value);
    }
}
