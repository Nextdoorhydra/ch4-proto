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
			"NKMSoundRuntime",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"ProceduralMeshComponent"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"CMGore",
			"ListenServerNetwork",
			"EnhancedInput",
			"InputCore",
			"PhysicsCore"
		});
	}
}
