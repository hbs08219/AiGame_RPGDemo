using UnrealBuildTool;

// DreamMaker generated minimal target. Keep Latest for installed-engine compatibility.
public class RpgDemoTarget : TargetRules
{
    public RpgDemoTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
    }
}
