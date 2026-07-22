using UnrealBuildTool;

public class RpgDemoEditorTarget : TargetRules
{
    public RpgDemoEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
    }
}
