using UnrealBuildTool;

public class RpgDemoTarget : TargetRules
{
    public RpgDemoTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
    }
}
