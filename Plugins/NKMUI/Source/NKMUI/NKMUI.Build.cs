using UnrealBuildTool;

public class NKMUI : ModuleRules
{
	public NKMUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"UMG",
				"CommonUI",
				"DeveloperSettings",
				"UIExtension"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CommonInput",
				"EnhancedInput",
				"InputCore",
				"Slate",
				"SlateCore"
			}
		);
	}
}
