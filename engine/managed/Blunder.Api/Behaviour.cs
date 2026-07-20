namespace Blunder;

/// <summary>
/// Unity-style gameplay script attached to a host <see cref="ObjectHandle"/>.
/// Not an Object subclass and not an ECS Component.
/// </summary>
public abstract class Behaviour
{
    public ObjectHandle Object { get; internal set; } = null!;

    /// <summary>Stable native BehaviourId (not a list index).</summary>
    public ulong BehaviourId { get; internal set; }

    public virtual void Ready() { }

    public virtual void Tick(float deltaTime) { }
}
