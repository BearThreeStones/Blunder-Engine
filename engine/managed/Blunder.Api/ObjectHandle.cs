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
    const string RotationProperty = "rotation";

    static readonly Dictionary<ulong, ObjectHandle> s_byId = new();

    readonly List<Behaviour> _behaviours = new();
    AnimationPlayer? _animationPlayer;
    AnimationTree? _animationTree;
    CharacterController? _characterController;

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

    public Quat Rotation
    {
        get
        {
            if (Native.blunder_object_get_quat_property(
                    Id,
                    ObjectClass,
                    RotationProperty,
                    out float x,
                    out float y,
                    out float z,
                    out float w) != Native.Ok)
            {
                return Quat.Identity;
            }

            return new Quat(x, y, z, w);
        }
        set =>
            Native.blunder_object_set_quat_property(
                Id, ObjectClass, RotationProperty, value.X, value.Y, value.Z, value.W);
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
                handle._animationPlayer?.DetachNativeListeners();
                handle._animationPlayer = null;
                handle._animationTree = null;
                handle._characterController = null;
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
        _animationPlayer?.DetachNativeListeners();
        _animationPlayer = null;
        _animationTree = null;
        int rc = Native.blunder_object_destroy(Id);
        lock (s_byId)
        {
            s_byId.Remove(Id);
            _behaviours.Clear();
        }

        return rc == Native.Ok;
    }

    public bool IsValid => Native.blunder_object_is_valid(Id) != 0;

    /// <summary>Cached co-located AnimationPlayer façade for this Object.</summary>
    public AnimationPlayer EnsureAnimationPlayer() =>
        _animationPlayer ??= new AnimationPlayer(this);

    /// <summary>Cached co-located AnimationTree façade for this Object.</summary>
    public AnimationTree EnsureAnimationTree() =>
        _animationTree ??= new AnimationTree(this);

    public CharacterController? CharacterController
    {
        get
        {
            if (Native.blunder_object_has_character_controller(Id, out int present) !=
                    Native.Ok ||
                present == 0)
            {
                return null;
            }

            return _characterController ??= new CharacterController(this);
        }
    }

    public void AddGroup(string name) => Native.blunder_object_add_group(Id, name);

    public void RemoveGroup(string name) => Native.blunder_object_remove_group(Id, name);

    public bool IsInGroup(string name)
    {
        if (Native.blunder_object_is_in_group(Id, name, out int value) != Native.Ok)
        {
            return false;
        }

        return value != 0;
    }

    public string[] Groups
    {
        get
        {
            int count = Native.blunder_object_group_count(Id);
            if (count <= 0)
            {
                return [];
            }

            string[] names = new string[count];
            for (int i = 0; i < count; ++i)
            {
                if (Native.blunder_object_group_at(Id, i, out string name) != Native.Ok)
                {
                    names[i] = "";
                    continue;
                }

                names[i] = name;
            }

            return names;
        }
    }

    public static ObjectHandle[] FindObjectsInGroup(string name) =>
        Physics.FindObjectsInGroup(name);

    public int SkeletonModifierCount
    {
        get
        {
            if (Native.blunder_skeleton_modifier_count(Id, out int count) != Native.Ok)
            {
                return 0;
            }

            return count;
        }
    }

    public bool SetSkeletonModifierEnabled(int index, bool enabled) =>
        Native.blunder_skeleton_modifier_set_enabled(Id, index, enabled ? 1 : 0) ==
        Native.Ok;

    public bool IsSkeletonModifierEnabled(int index)
    {
        if (Native.blunder_skeleton_modifier_get_enabled(Id, index, out int enabled) !=
            Native.Ok)
        {
            return false;
        }

        return enabled != 0;
    }

    public bool MoveSkeletonModifier(int fromIndex, int toIndex) =>
        Native.blunder_skeleton_modifier_move(Id, fromIndex, toIndex) == Native.Ok;

    /// <summary>Lean PaperMouth drive façade at <paramref name="index"/>.</summary>
    public PaperMouth PaperMouthAt(int index) => new(this, index);

    /// <summary>Lean SkeletonAttachModifier drive façade at <paramref name="index"/>.</summary>
    public SkeletonAttachModifier AttachAt(int index) => new(this, index);

    /// <summary>Lean LookAt drive façade at <paramref name="index"/>.</summary>
    public LookAt LookAtAt(int index) => new(this, index);
}
