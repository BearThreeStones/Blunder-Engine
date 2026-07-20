namespace Blunder;

/// <summary>
/// Managed façade over a native Object id. Sibling Behaviours share one
/// canonical handle per ObjectId so <see cref="GetBehaviour{T}"/> works for
/// both <see cref="AddBehaviour{T}"/> and ScriptHost AttachBehaviour.
/// </summary>
public sealed class ObjectHandle
{
    const string ObjectClass = "Object";
    const string PositionProperty = "position";

    static readonly Dictionary<ulong, ObjectHandle> s_byId = new();

    readonly List<Behaviour> _behaviours = new();

    ObjectHandle(ulong id)
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
    /// Returns the canonical handle for <paramref name="id"/>, creating one if
    /// needed so AttachBehaviour and AddBehaviour share sibling lists.
    /// </summary>
    public static ObjectHandle GetOrCreate(ulong id)
    {
        if (id == 0)
        {
            throw new ArgumentOutOfRangeException(nameof(id));
        }

        lock (s_byId)
        {
            if (s_byId.TryGetValue(id, out ObjectHandle? existing))
            {
                return existing;
            }

            ObjectHandle created = new(id);
            s_byId[id] = created;
            return created;
        }
    }

    /// <summary>Registers a managed Behaviour instance for sibling queries.</summary>
    internal void RegisterBehaviour(Behaviour behaviour)
    {
        _behaviours.Add(behaviour);
    }

    /// <summary>Drops all canonical handles (host shutdown / test teardown).</summary>
    internal static void ClearRegistry()
    {
        lock (s_byId)
        {
            foreach (ObjectHandle handle in s_byId.Values)
            {
                handle._behaviours.Clear();
            }

            s_byId.Clear();
        }
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
        RegisterBehaviour(behaviour);
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

    public static ObjectHandle Create() => GetOrCreate(Native.blunder_object_create());

    public bool Destroy()
    {
        int rc = Native.blunder_object_destroy(Id);
        lock (s_byId)
        {
            s_byId.Remove(Id);
            _behaviours.Clear();
        }

        return rc == Native.Ok;
    }

    public bool IsValid => Native.blunder_object_is_valid(Id) != 0;
}
