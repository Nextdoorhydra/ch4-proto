#pragma once

#include "CoreMinimal.h"
#include "GameFlow/CMPlayPhase.h"

#include "CMGameFlowTypes.generated.h"

UENUM(BlueprintType)
// 로비 대기·로딩 흐름 상태 구분
enum class ECMLobbyPhase : uint8
{
    Waiting,
    Loading
};

UENUM(BlueprintType)
// 월드가 로컬 플레이어에게 표시 가능한 상태인지 구분
enum class ECMWorldPresentationState : uint8
{
    Ready,
    Loading,
    Failed
};
