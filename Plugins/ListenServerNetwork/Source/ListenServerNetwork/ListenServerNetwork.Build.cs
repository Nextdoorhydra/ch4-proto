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
			"CoreOnline",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"Projects"
		});

		bool bSteamworksSupported = Target.Platform == UnrealTargetPlatform.Win64
			|| Target.Platform == UnrealTargetPlatform.Linux
			|| Target.Platform == UnrealTargetPlatform.Mac;
		PrivateDefinitions.Add("WITH_STEAMWORKS=" + (bSteamworksSupported ? "1" : "0"));
		if (bSteamworksSupported)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");
		}
	}
}
