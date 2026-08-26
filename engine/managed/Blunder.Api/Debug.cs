using System.Diagnostics;

namespace Blunder;

/// <summary>
/// Gameplay diagnostics → Console Messages (not System.Console).
/// </summary>
public static class Debug
{
    public const int SeverityLog = 0;
    public const int SeverityWarning = 1;
    public const int SeverityError = 2;

    public static void Log(string message) =>
        Write(SeverityLog, message);

    public static void LogWarning(string message) =>
        Write(SeverityWarning, message);

    public static void LogError(string message) =>
        Write(SeverityError, message);

    internal static void LogErrorCaptured(string message, string? stack)
    {
        _ = Native.blunder_log(SeverityError, message ?? "", stack);
    }

    static void Write(int severity, string message)
    {
        string stack = new StackTrace(1, fNeedFileInfo: true).ToString();
        _ = Native.blunder_log(severity, message ?? "", stack);
    }
}
