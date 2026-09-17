#include "Sound/NKMSoundHelper.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Sound/NKMSoundSubsystem.h"

UNKMSoundSubsystem* FNKMSoundHelper::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	return GameInstance->GetSubsystem<UNKMSoundSubsystem>();
}
