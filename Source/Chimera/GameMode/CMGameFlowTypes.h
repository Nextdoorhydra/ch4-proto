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
