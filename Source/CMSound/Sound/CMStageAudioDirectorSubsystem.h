#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameFramework/GameplayMessageSubsystem.h"

#include "CMStageAudioDirectorSubsystem.generated.h"

struct FCMPlayStateMessage;

UCLASS()
// 복제된 플레이 상태를 각 머신의 로컬 BGM 재생으로 변환
class CMSOUND_API UCMStageAudioDirectorSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;

private:
    void HandlePlayStateChanged(FGameplayTag Channel, const FCMPlayStateMessage& Message);

    FGameplayMessageListenerHandle StateChangedHandle;
};
