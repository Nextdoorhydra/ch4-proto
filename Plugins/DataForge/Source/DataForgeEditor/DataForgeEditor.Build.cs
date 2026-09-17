using UnrealBuildTool;

public class DataForgeEditor : ModuleRules
{
	public DataForgeEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DataForgeCore"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"AssetRegistry",
			"AssetTools",
			"ContentBrowser",
			"DirectoryWatcher",
			"InputCore",
			"Json",
			"MessageLog",
			"PropertyEditor",
			"Projects",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"UnrealEd"
		});
	}
}
