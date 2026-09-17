#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Stage/Device/CMPartLoadoutStorageSubsystem.h"

#include "CMPartLoadoutSaveGame.generated.h"

/** 치트 명령으로 저장한 파츠 프리셋 10개를 디스크에 보존한다. */
UCLASS()
class CHIMERA_API UCMPartLoadoutSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(SaveGame)
    TArray<FCMStoredPartLoadout> StoredLoadouts;
};
