namespace Blunder;

/// <summary>
/// Quaternion matching engine <c>glm::quat</c> components (.x/.y/.z/.w).
/// C-ABI passes those components; native constructs <c>Quat(w, x, y, z)</c>.
/// </summary>
public readonly struct Quat : IEquatable<Quat>
{
    public float X { get; }
    public float Y { get; }
    public float Z { get; }
    public float W { get; }

    public static Quat Identity => new(0f, 0f, 0f, 1f);

    public Quat(float x, float y, float z, float w)
    {
        X = x;
        Y = y;
        Z = z;
        W = w;
    }

    public Quat Normalized
    {
        get
        {
            float length = MathF.Sqrt((X * X) + (Y * Y) + (Z * Z) + (W * W));
            if (length < 1e-8f)
            {
                return Identity;
            }

            return new Quat(X / length, Y / length, Z / length, W / length);
        }
    }

    /// <summary>Rotation of <paramref name="radians"/> about <paramref name="axis"/>.</summary>
    public static Quat AngleAxis(float radians, Vec3 axis)
    {
        float axisLength = MathF.Sqrt((axis.X * axis.X) + (axis.Y * axis.Y) + (axis.Z * axis.Z));
        if (axisLength < 1e-8f)
        {
            return Identity;
        }

        float nx = axis.X / axisLength;
        float ny = axis.Y / axisLength;
        float nz = axis.Z / axisLength;
        float half = radians * 0.5f;
        float s = MathF.Sin(half);
        float c = MathF.Cos(half);
        return new Quat(nx * s, ny * s, nz * s, c);
    }

    public bool Equals(Quat other) =>
        X == other.X && Y == other.Y && Z == other.Z && W == other.W;

    public override bool Equals(object? obj) => obj is Quat other && Equals(other);

    public override int GetHashCode() => HashCode.Combine(X, Y, Z, W);

    public override string ToString() => $"({X}, {Y}, {Z}, {W})";

    public static bool operator ==(Quat left, Quat right) => left.Equals(right);

    public static bool operator !=(Quat left, Quat right) => !left.Equals(right);
}
