using UnrealBuildTool;

public class RpgDemoEditorTarget : TargetRules
{
    public RpgDemoEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        BuildEnvironment = TargetBuildEnvironment.Unique;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
    }
}
