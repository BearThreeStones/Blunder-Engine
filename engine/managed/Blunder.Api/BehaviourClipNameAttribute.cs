namespace Blunder;

/// <summary>
/// Marks a Behaviour string field/property as a weak logical clip-name reference
/// into the co-located AnimationPlayer Clip Binding map. The Scripts catalog
/// emits <c>kind: "clip_name"</c> so the Inspector shows a name dropdown.
/// </summary>
[AttributeUsage(AttributeTargets.Field | AttributeTargets.Property,
                AllowMultiple = false, Inherited = true)]
public sealed class BehaviourClipNameAttribute : Attribute
{
}
