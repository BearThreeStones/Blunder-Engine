namespace Blunder;

/// <summary>
/// Managed façade for an Object's co-located AnimationTree (C-ABI v8).
/// </summary>
public sealed class AnimationTree
{
    readonly ObjectHandle _owner;

    internal AnimationTree(ObjectHandle owner) => _owner = owner;

    public bool Active
    {
        get
        {
            if (Native.blunder_animation_tree_get_active(_owner.Id, out int value) !=
                Native.Ok)
            {
                return false;
            }

            return value != 0;
        }
        set => Native.blunder_animation_tree_set_active(_owner.Id, value ? 1 : 0);
    }

    public bool Travel(string stateName) =>
        Native.blunder_animation_tree_travel(_owner.Id, stateName) == Native.Ok;

    public bool Start(string stateName) =>
        Native.blunder_animation_tree_start(_owner.Id, stateName) == Native.Ok;

    public void SetBlendSpaceScalar(string nodeName, float scalar) =>
        Native.blunder_animation_tree_set_blend_space_scalar(_owner.Id, nodeName, scalar);

    public float GetBlendSpaceScalar(string nodeName)
    {
        if (Native.blunder_animation_tree_get_blend_space_scalar(
                _owner.Id, nodeName, out float value) != Native.Ok)
        {
            return 0f;
        }

        return value;
    }

    public bool RequestOneShot(string clipName) =>
        Native.blunder_animation_tree_request_one_shot(_owner.Id, clipName) == Native.Ok;

    public float Add2Weight
    {
        get
        {
            if (Native.blunder_animation_tree_get_add2_weight(_owner.Id, out float value) !=
                Native.Ok)
            {
                return 0f;
            }

            return value;
        }
        set => Native.blunder_animation_tree_set_add2_weight(_owner.Id, value);
    }
}
