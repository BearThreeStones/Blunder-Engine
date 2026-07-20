namespace Blunder;

/// <summary>
/// Managed façade over a native Object id. Sibling Behaviours are tracked in-process
/// so <see cref="GetBehaviour{T}"/> / <see cref="GetBehaviours{T}"/> work before ScriptHost
/// owns GCHandle peers (Task 5).
/// </summary>
public sealed class ObjectHandle
{
    const string ObjectClass = "Object";
    const string PositionProperty = "position";

    readonly List<Behaviour> _behaviours = new();

    public ObjectHandle(ulong id)
    {
        Id = id;
    }

    public ulong Id { get; }

    public Vec3 Position
    {
        get
        {
            if (Native.blunder_object_get_vec3_property(
                    Id, ObjectClass, PositionProperty, out float x, out float y, out float z) !=
                Native.Ok)
            {
                return default;
            }

            return new Vec3(x, y, z);
        }
        set =>
            Native.blunder_object_set_vec3_property(
                Id, ObjectClass, PositionProperty, value.X, value.Y, value.Z);
    }

    /// <summary>
    /// Registers a Behaviour slot natively (type = <see cref="Type.FullName"/>) and
    /// constructs a managed instance for sibling queries.
    /// </summary>
    public ulong AddBehaviour<T>() where T : Behaviour, new()
    {
        // FullName for MVP: one game assembly load context (no AssemblyQualifiedName).
        string typeName = typeof(T).FullName ?? typeof(T).Name;
        ulong behaviourId = Native.blunder_object_add_behaviour(Id, typeName);
        if (behaviourId == 0)
        {
            return 0;
        }

        T behaviour = new()
        {
            Object = this,
            BehaviourId = behaviourId,
        };
        _behaviours.Add(behaviour);
        return behaviourId;
    }

    public T? GetBehaviour<T>() where T : Behaviour
    {
        foreach (Behaviour behaviour in _behaviours)
        {
            if (behaviour is T typed)
            {
                return typed;
            }
        }

        return null;
    }

    public T[] GetBehaviours<T>() where T : Behaviour
    {
        List<T> matches = new();
        foreach (Behaviour behaviour in _behaviours)
        {
            if (behaviour is T typed)
            {
                matches.Add(typed);
            }
        }

        return matches.ToArray();
    }

    public static ObjectHandle Create() => new(Native.blunder_object_create());

    public bool Destroy() => Native.blunder_object_destroy(Id) == Native.Ok;

    public bool IsValid => Native.blunder_object_is_valid(Id) != 0;
}
