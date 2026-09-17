namespace Blunder;

/// <summary>
/// Physics query result in SI metres. Groups are entity strings; Object is
/// set only when the hit entity has a bound Object.
/// </summary>
public readonly struct PhysicsHit
{
    public bool Hit { get; init; }
    public bool IsArea { get; init; }
    public float Distance { get; init; }
    public Vec3 Point { get; init; }
    public Vec3 Normal { get; init; }
    public ObjectHandle? Object { get; init; }
    public string[] Groups { get; init; }
}

/// <summary>
/// Play/Edit physics queries through the registered NativeAbi table.
/// </summary>
public static unsafe class Physics
{
    public static bool Raycast(
        Vec3 origin, Vec3 direction, float maxDistance, uint mask,
        bool collideWithAreas, out PhysicsHit hit)
    {
        BlunderPhysicsRay ray = new()
        {
            ox = origin.X,
            oy = origin.Y,
            oz = origin.Z,
            dx = direction.X,
            dy = direction.Y,
            dz = direction.Z,
            max_distance = maxDistance,
            mask = mask,
            collide_with_areas = collideWithAreas ? 1 : 0,
        };
        int rc = Native.blunder_physics_raycast(in ray, out BlunderPhysicsHit native);
        hit = FromNative(native);
        return rc == Native.Ok && hit.Hit;
    }

    public static bool BoxCast(
        Vec3 origin, Quat rotation, Vec3 halfExtents, Vec3 direction,
        float maxDistance, uint mask, bool collideWithAreas, out PhysicsHit hit)
    {
        return Shapecast(0, origin, rotation, halfExtents, 0f, 0f, 0f, direction,
                         maxDistance, mask, collideWithAreas, out hit);
    }

    public static bool SphereCast(
        Vec3 origin, Quat rotation, float radius, Vec3 direction, float maxDistance,
        uint mask, bool collideWithAreas, out PhysicsHit hit)
    {
        return Shapecast(1, origin, rotation, default, radius, 0f, 0f, direction,
                         maxDistance, mask, collideWithAreas, out hit);
    }

    public static bool CapsuleCast(
        Vec3 origin, Quat rotation, float radius, float halfHeight, Vec3 direction,
        float maxDistance, uint mask, bool collideWithAreas, out PhysicsHit hit)
    {
        return Shapecast(2, origin, rotation, default, 0f, radius, halfHeight, direction,
                         maxDistance, mask, collideWithAreas, out hit);
    }

    public static ObjectHandle[] FindObjectsInGroup(string name)
    {
        if (Native.blunder_find_objects_in_group(name, out ulong[] ids) != Native.Ok ||
            ids.Length == 0)
        {
            return [];
        }

        ObjectHandle[] handles = new ObjectHandle[ids.Length];
        for (int i = 0; i < ids.Length; ++i)
        {
            handles[i] = ObjectHandle.GetOrCreate(ids[i]);
        }

        return handles;
    }

    static bool Shapecast(
        int shape, Vec3 origin, Quat rotation, Vec3 halfExtents, float sphereRadius,
        float capsuleRadius, float capsuleHalfHeight, Vec3 direction, float maxDistance,
        uint mask, bool collideWithAreas, out PhysicsHit hit)
    {
        BlunderPhysicsSweep sweep = new()
        {
            shape = shape,
            ox = origin.X,
            oy = origin.Y,
            oz = origin.Z,
            qx = rotation.X,
            qy = rotation.Y,
            qz = rotation.Z,
            qw = rotation.W,
            hx = halfExtents.X,
            hy = halfExtents.Y,
            hz = halfExtents.Z,
            sphere_radius = sphereRadius,
            capsule_radius = capsuleRadius,
            capsule_half_height = capsuleHalfHeight,
            dx = direction.X,
            dy = direction.Y,
            dz = direction.Z,
            max_distance = maxDistance,
            mask = mask,
            collide_with_areas = collideWithAreas ? 1 : 0,
        };
        int rc = Native.blunder_physics_shapecast(in sweep, out BlunderPhysicsHit native);
        hit = FromNative(native);
        return rc == Native.Ok && hit.Hit;
    }

    static PhysicsHit FromNative(in BlunderPhysicsHit native)
    {
        string groupsCsv;
        fixed (byte* groupsBytes = native.groups)
        {
            int length = 0;
            while (length < 255 && groupsBytes[length] != 0)
            {
                ++length;
            }

            groupsCsv = length == 0
                ? ""
                : System.Text.Encoding.UTF8.GetString(groupsBytes, length);
        }

        string[] groups = string.IsNullOrEmpty(groupsCsv)
            ? []
            : groupsCsv.Split(',', StringSplitOptions.RemoveEmptyEntries);

        ObjectHandle? obj = null;
        if (native.object_id != 0)
        {
            obj = ObjectHandle.GetOrCreate(native.object_id);
        }

        return new PhysicsHit
        {
            Hit = native.hit != 0,
            IsArea = native.is_area != 0,
            Distance = native.distance,
            Point = new Vec3(native.point_x, native.point_y, native.point_z),
            Normal = new Vec3(native.normal_x, native.normal_y, native.normal_z),
            Object = obj,
            Groups = groups,
        };
    }
}
