using UnrealBuildTool;

public class ChimeraEditor : ModuleRules
{
    public ChimeraEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.Add(ModuleDirectory);

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
                "UnrealEd"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "AssetRegistry",
				"InputCore",
				"Json",
                "Slate",
                "SlateCore"
            }
        );
    }
}
