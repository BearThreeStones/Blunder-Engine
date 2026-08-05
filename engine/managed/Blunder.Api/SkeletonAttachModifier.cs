namespace Blunder;

/// <summary>
/// Managed façade for a SkeletonAttachModifier at <see cref="Index"/> (C-ABI v10).
/// </summary>
public sealed class SkeletonAttachModifier
{
    readonly ObjectHandle _owner;

    internal SkeletonAttachModifier(ObjectHandle owner, int index)
    {
        _owner = owner;
        Index = index;
    }

    public int Index { get; }

    public ObjectHandle? Child
    {
        get
        {
            if (Native.blunder_skeleton_modifier_get_attach_child_object_id(
                    _owner.Id, Index, out ulong childId) != Native.Ok ||
                childId == 0)
            {
                return null;
            }

            return ObjectHandle.GetOrCreate(childId);
        }
        set
        {
            ulong childId = value?.Id ?? 0;
            Native.blunder_skeleton_modifier_set_attach_child_object_id(
                _owner.Id, Index, childId);
        }
    }

    public string BoneName
    {
        get
        {
            if (Native.blunder_skeleton_modifier_get_attach_bone_name(
                    _owner.Id, Index, out string boneName) != Native.Ok)
            {
                return "";
            }

            return boneName;
        }
        set =>
            Native.blunder_skeleton_modifier_set_attach_bone_name(
                _owner.Id, Index, value ?? "");
    }
}
