namespace DotnetHostGame;

/// <summary>
/// Second fixture Behaviour: asserts GetBehaviour finds ProbeBehaviour after
/// both are attached via ScriptHost (sibling registry).
/// </summary>
public sealed class SiblingProbeBehaviour : Blunder.Behaviour
{
    /// <summary>1 if <see cref="ProbeBehaviour"/> was found on Ready; else 0.</summary>
    public static int FoundSibling;

    public override void Ready()
    {
        FoundSibling = Object.GetBehaviour<ProbeBehaviour>() != null ? 1 : 0;
    }
}
