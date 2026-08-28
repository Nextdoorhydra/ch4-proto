using UnrealBuildTool;

public class CameraOcclusionEditor : ModuleRules
{
    public CameraOcclusionEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.Add("Core");
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "CoreUObject",
            "Engine",
            "UnrealEd",
            "AssetRegistry",
            "MaterialEditor",
            "ContentBrowser",
            "ToolMenus",
            "Slate",
            "SlateCore"
        });
    }
}

