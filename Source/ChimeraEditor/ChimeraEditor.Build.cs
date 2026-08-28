using UnrealBuildTool;

public class ChimeraEditor : ModuleRules
{
    public ChimeraEditor(ReadOnlyTargetRules Target) : base(Target)
    {
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.Add(ModuleDirectory);
		PrivateIncludePaths.Add(ModuleDirectory + "/../CameraOcclusionEditor");

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "Chimera",
                "DataForgeCore",
                "DataForgeEditor",
                "GoogleSheetLoader", 
				"AsyncPDALoader",
				"PropertyEditor",
				"UnrealEd",
				"ContentBrowser",
				"ToolMenus",
				"MaterialEditor"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "CameraOcclusionEditor",
                "AssetRegistry",
				"InputCore",
				"Json",
                "Slate",
                "SlateCore"
            }
        );
    }
}
