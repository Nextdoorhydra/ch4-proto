using UnrealBuildTool;

public class NKMLocalization : ModuleRules
{
	public NKMLocalization(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
				"Core",
				"CoreUObject",
				"DeveloperSettings",
				"Engine"
			});
	}
}
