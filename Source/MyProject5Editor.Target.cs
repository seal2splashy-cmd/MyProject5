using UnrealBuildTool;
using System.Collections.Generic;

public class MyProject5EditorTarget : TargetRules
{
    public MyProject5EditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("MyProject5");
    }
}
