namespace Blunder;

/// <summary>
/// Managed façade for an Object's co-located AnimationTree (C-ABI v12).
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

    public string AssetGuid
    {
        get
        {
            if (Native.blunder_animation_tree_get_asset_guid(_owner.Id, out string guid) !=
                Native.Ok)
            {
                return "";
            }

            return guid;
        }
        set => Native.blunder_animation_tree_set_asset_guid(_owner.Id, value ?? "");
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

    public void SetBlendSpace2DParam(string nodeName, float x, float y) =>
        Native.blunder_animation_tree_set_blend_space_2d_param(_owner.Id, nodeName, x, y);

    public (float X, float Y) GetBlendSpace2DParam(string nodeName)
    {
        if (Native.blunder_animation_tree_get_blend_space_2d_param(
                _owner.Id, nodeName, out float x, out float y) != Native.Ok)
        {
            return (0f, 0f);
        }

        return (x, y);
    }

    public bool RequestOneShot(string clipName) =>
        Native.blunder_animation_tree_request_one_shot(_owner.Id, clipName) == Native.Ok;

    /// <summary>
    /// Clip Play: replace the tree base with a Clip Binding logical name (hard cut, clock 0).
    /// </summary>
    public bool Play(string clipName) =>
        Native.blunder_animation_tree_play(_owner.Id, clipName) == Native.Ok;

    public void SetTreeParamBool(string name, bool value) =>
        Native.blunder_animation_tree_set_tree_param_bool(_owner.Id, name, value ? 1 : 0);

    public bool GetTreeParamBool(string name)
    {
        if (Native.blunder_animation_tree_get_tree_param_bool(
                _owner.Id, name, out int value) != Native.Ok)
        {
            return false;
        }

        return value != 0;
    }

    public void SetTreeParamFloat(string name, float value) =>
        Native.blunder_animation_tree_set_tree_param_float(_owner.Id, name, value);

    public float GetTreeParamFloat(string name)
    {
        if (Native.blunder_animation_tree_get_tree_param_float(
                _owner.Id, name, out float value) != Native.Ok)
        {
            return 0f;
        }

        return value;
    }

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
