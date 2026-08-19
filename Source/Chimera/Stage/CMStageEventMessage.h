#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "CMStageEventMessage.generated.h"

USTRUCT(BlueprintType)
struct FCMStageEventMessage
{
    GENERATED_BODY()

    // Travel과 재시작 사이의 이전 이벤트를 구분하는 스테이지 실행 식별자
    UPROPERTY(BlueprintReadOnly)
    FGuid StageInstanceId;

    // 퍼즐 완료, 장애물 발동, 조명 변경 등 이벤트 의미
    UPROPERTY(BlueprintReadOnly)
    FGameplayTag EventTag;

    // 이벤트를 발생시킨 선택적 월드 객체
    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<UObject> Instigator;
};
