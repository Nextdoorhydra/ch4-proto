using UnrealBuildTool;

public class NKMSoundRuntime : ModuleRules
{
	public NKMSoundRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"DeveloperSettings",
				"Engine",
				"GameplayTags"
			}
		);
	}
}
