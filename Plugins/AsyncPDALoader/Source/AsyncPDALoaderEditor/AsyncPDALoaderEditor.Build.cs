using UnrealBuildTool;

public class AsyncPDALoaderEditor : ModuleRules
{
	public AsyncPDALoaderEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"AsyncPDALoader",
				"GameplayTags",
				"GameplayMessageRuntime",
				"DataValidation",
				"AssetRegistry",
				"UnrealEd"
			}
		);
	}
}

