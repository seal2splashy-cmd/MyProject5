using UnrealBuildTool;

public class MyProject5 : ModuleRules
{
    public MyProject5(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine",
            "InputCore", "EnhancedInput" });

        PrivateDependencyModuleNames.AddRange(new string[] {
            "Json" });
    }
}
