#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPath.h"

#include "ListenServerNetworkSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Listen Server Network"))
class LISTENSERVERNETWORK_API UListenServerNetworkSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UListenServerNetworkSettings();

	UPROPERTY(Config, EditAnywhere, Category="Compatibility")
	FString ProjectKey;

	UPROPERTY(Config, EditAnywhere, Category="Compatibility", meta=(ClampMin="1"))
	int32 BuildUniqueId = 1;

	UPROPERTY(Config, EditAnywhere, Category="Defaults", meta=(ClampMin="1"))
	int32 DefaultMaxPlayers = 4;

	UPROPERTY(Config, EditAnywhere, Category="Defaults", meta=(ClampMin="1"))
	int32 DefaultMaxSearchResults = 50;

	UPROPERTY(Config, EditAnywhere, Category="Defaults")
	FName DefaultGameModeId;

	UPROPERTY(Config, EditAnywhere, Category="Defaults")
	FName DefaultRegion;

	UPROPERTY(Config, EditAnywhere, Category="Defaults")
	FString DefaultSessionDisplayName;

	UPROPERTY(Config, EditAnywhere, Category="Maps", meta=(AllowedClasses="/Script/Engine.World"))
	FSoftObjectPath MainMenuMap;

	UPROPERTY(Config, EditAnywhere, Category="Maps", meta=(AllowedClasses="/Script/Engine.World"))
	FSoftObjectPath LobbyMap;

	UPROPERTY(Config, EditAnywhere, Category="Maps", meta=(AllowedClasses="/Script/Engine.World"))
	FSoftObjectPath DefaultGameMap;

	UPROPERTY(Config, EditAnywhere, Category="Timeouts", meta=(ClampMin="1.0"))
	float CreateTimeoutSeconds = 30.0f;

	UPROPERTY(Config, EditAnywhere, Category="Timeouts", meta=(ClampMin="1.0"))
	float SearchTimeoutSeconds = 20.0f;

	UPROPERTY(Config, EditAnywhere, Category="Timeouts", meta=(ClampMin="1.0"))
	float JoinTimeoutSeconds = 30.0f;

	UPROPERTY(Config, EditAnywhere, Category="Timeouts", meta=(ClampMin="1.0"))
	float UpdateTimeoutSeconds = 15.0f;

	UPROPERTY(Config, EditAnywhere, Category="Timeouts", meta=(ClampMin="1.0"))
	float DestroyTimeoutSeconds = 15.0f;

	UPROPERTY(Config, EditAnywhere, Category="Timeouts", meta=(ClampMin="1.0"))
	float TravelTimeoutSeconds = 45.0f;

	UPROPERTY(Config, EditAnywhere, Category="Session")
	bool bAllowJoinInProgress = true;

	UPROPERTY(Config, EditAnywhere, Category="Diagnostics")
	bool bEnableVerboseLogging = false;
};
