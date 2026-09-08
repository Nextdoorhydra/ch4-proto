using UnrealBuildTool;

public class CMSound : ModuleRules
{
	public CMSound(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.Add(ModuleDirectory);
		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core", "CoreUObject", "Engine", "GameplayTags", "Shared",
			"GameplayMessageRuntime", "AsyncPDALoader", "NKMSoundRuntime"
		});
	}
}
