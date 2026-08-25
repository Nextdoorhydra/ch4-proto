using UnrealBuildTool;

public class AI : ModuleRules
{
	public AI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Learning",
			"LearningAgents",
			"LearningAgentsTraining",
			"LearningTraining",
			"NavigationSystem",
			"Shared",
			"Chimera"
		});
	}
}
