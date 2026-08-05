using UnrealBuildTool;

public class ListenServerNetwork : ModuleRules
{
	public ListenServerNetwork(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"DeveloperSettings",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"Projects"
		});
	}
}
