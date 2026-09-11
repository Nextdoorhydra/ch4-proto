// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Chimera : ModuleRules
{
	public Chimera(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.Add(ModuleDirectory);
	
		PublicDependencyModuleNames.AddRange(new[]
		{
			"AsyncPDALoader",
			"CableComponent",
			"Core",
			"CoreUObject",
			"DeveloperSettings",
			"Engine",
			"GameplayAbilities",
			"GameplayMessageRuntime",
			"GameplayTags",
			"GameplayTasks",
			"Niagara",
			"Shared",
			"OnlineSubsystem",
			"OnlineSubsystemUtils"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"CMGore",
			"CMSound",
			"NKMSoundRuntime",
			"ListenServerNetwork",
			"EnhancedInput",
			"InputCore",
			"PhysicsCore",
			"Slate",
			"SlateCore",
			"UMG"
		});
	}
}
