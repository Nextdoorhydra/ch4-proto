using UnrealBuildTool;

public class NKMLocalizationEditor : ModuleRules
{
	public NKMLocalizationEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"NKMLocalization"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
				"DesktopPlatform",
				"InputCore",
				"Json",
				"LevelEditor",
				"Localization",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"TranslationEditor",
				"UnrealEd",
				"WorkspaceMenuStructure"
			}
		);
	}
}
