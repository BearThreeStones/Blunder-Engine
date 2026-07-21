namespace Blunder;

/// <summary>Float2 for Gameplay Input Move (world +X / +Y plane).</summary>
public readonly struct Vec2 : IEquatable<Vec2>
{
    public float X { get; }
    public float Y { get; }

    public Vec2(float x, float y)
    {
        X = x;
        Y = y;
    }

    public bool Equals(Vec2 other) => X == other.X && Y == other.Y;

    public override bool Equals(object? obj) => obj is Vec2 other && Equals(other);

    public override int GetHashCode() => HashCode.Combine(X, Y);

    public override string ToString() => $"({X}, {Y})";

    public static bool operator ==(Vec2 left, Vec2 right) => left.Equals(right);

    public static bool operator !=(Vec2 left, Vec2 right) => !left.Equals(right);
}
