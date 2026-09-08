#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "CMGameSoundBridgeSubsystem.generated.h"

class UNKMSoundSubsystem;
struct FAsyncLoadCompleteMessage;

UCLASS()
// AsyncPDALoader의 현재 캐시를 태그 기반 사운드 조회표로 변환
class CMSOUND_API UCMGameSoundBridgeSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

private:
    void HandleLoadComplete(FGameplayTag Channel, const FAsyncLoadCompleteMessage& Message);
    void RebuildRegisteredSoundCatalogs();

    FGameplayMessageListenerHandle LoadCompleteListenerHandle;

    UPROPERTY()
    TObjectPtr<UNKMSoundSubsystem> SoundSubsystem;
};
