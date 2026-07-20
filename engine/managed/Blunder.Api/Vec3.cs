namespace Blunder;

/// <summary>Float3 matching the engine C-ABI / native Vec3 layout.</summary>
public readonly struct Vec3 : IEquatable<Vec3>
{
    public float X { get; }
    public float Y { get; }
    public float Z { get; }

    public Vec3(float x, float y, float z)
    {
        X = x;
        Y = y;
        Z = z;
    }

    public bool Equals(Vec3 other) => X == other.X && Y == other.Y && Z == other.Z;

    public override bool Equals(object? obj) => obj is Vec3 other && Equals(other);

    public override int GetHashCode() => HashCode.Combine(X, Y, Z);

    public override string ToString() => $"({X}, {Y}, {Z})";

    public static bool operator ==(Vec3 left, Vec3 right) => left.Equals(right);

    public static bool operator !=(Vec3 left, Vec3 right) => !left.Equals(right);
}
