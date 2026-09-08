#pragma once

#include "CoreMinimal.h"
#include "GameFlow/CMPlayPhase.h"
#include "NativeGameplayTags.h"

#include "CMPlayStateMessages.generated.h"

class UWorld;

namespace CMPlayStateMessages
{
    SHARED_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(StateChanged);
    SHARED_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(RequestCurrentState);
}

// 로컬 GameInstance 메시지. 네트워크 전달은 기존 GameState 복제가 담당한다.
USTRUCT()
struct SHARED_API FCMPlayStateMessage
{
    GENERATED_BODY()

    UPROPERTY()
    TObjectPtr<UWorld> World = nullptr;

    UPROPERTY()
    ECMPlayPhase Phase = ECMPlayPhase::Loading;

    UPROPERTY()
    int32 StageIndex = 0;

    UPROPERTY()
    FGameplayTag StageBGMTag;
};

USTRUCT()
struct SHARED_API FCMPlayStateRequest
{
    GENERATED_BODY()

    UPROPERTY()
    TObjectPtr<UWorld> World = nullptr;
};
