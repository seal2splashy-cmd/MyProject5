using UnrealBuildTool;
using System.Collections.Generic;

public class MyProject5Target : TargetRules
{
    public MyProject5Target(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("MyProject5");
    }
}
