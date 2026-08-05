namespace Blunder;

/// <summary>
/// Managed façade for a PaperMouth SkeletonModifier at <see cref="Index"/> (C-ABI v10).
/// </summary>
public sealed class PaperMouth
{
    readonly ObjectHandle _owner;

    internal PaperMouth(ObjectHandle owner, int index)
    {
        _owner = owner;
        Index = index;
    }

    public int Index { get; }

    public float OpenAmount
    {
        get
        {
            if (Native.blunder_skeleton_modifier_get_paper_mouth_open_amount(
                    _owner.Id, Index, out float value) != Native.Ok)
            {
                return 0f;
            }

            return value;
        }
        set =>
            Native.blunder_skeleton_modifier_set_paper_mouth_open_amount(
                _owner.Id, Index, value);
    }

    public string BoneName
    {
        get
        {
            if (Native.blunder_skeleton_modifier_get_paper_mouth_bone_name(
                    _owner.Id, Index, out string boneName) != Native.Ok)
            {
                return "";
            }

            return boneName;
        }
        set =>
            Native.blunder_skeleton_modifier_set_paper_mouth_bone_name(
                _owner.Id, Index, value ?? "");
    }
}
