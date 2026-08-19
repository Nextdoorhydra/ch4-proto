using UnrealBuildTool;

public class AsyncPDALoader : ModuleRules
{
	public AsyncPDALoader(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"GameplayMessageRuntime",
				"DeveloperSettings"
			}
		);
	}
}

