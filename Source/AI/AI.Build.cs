using UnrealBuildTool;

public class AI : ModuleRules
{
	public AI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new[]
		{
			"AIModule",
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"Learning",
			"LearningAgents",
			"LearningAgentsTraining",
			"LearningTraining",
			"MotionWarping",
			"NavigationSystem",
			"Shared",
			"Chimera"
		});
	}
}
