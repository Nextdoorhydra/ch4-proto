using UnrealBuildTool;

public class Gameplay : ModuleRules
{
	public Gameplay(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Shared"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"EnhancedInput",
			"InputCore",
			"ListenServerNetwork"
		});
	}
}
