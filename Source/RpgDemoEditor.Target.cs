using UnrealBuildTool;

// DreamMaker generated minimal target. Keep Latest for installed-engine compatibility.
public class RpgDemoEditorTarget : TargetRules
{
    public RpgDemoEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
    }
}
