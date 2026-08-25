#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "Aggressive/Common/Core/CMAggressiveAITypes.h"

#include "CMAggressiveMovementAgent.generated.h"

class UPrimitiveComponent;

/** 몸체 형태와 이동 방식에 관계없이 경로와 이동 컴포넌트가 사용하는 공격적 AI 계약이다. */
UINTERFACE(MinimalAPI)
class UCMAggressiveMovementAgent : public UInterface
{
    GENERATED_BODY()
};

/** 공격적 AI의 물리 몸통과 내비게이션 기준 위치를 공통으로 제공한다. */
class AI_API ICMAggressiveMovementAgent
{
    GENERATED_BODY()

public:
    virtual UPrimitiveComponent* GetAggressiveMovementBody() const = 0;
    virtual FVector GetAggressiveNavigationReferenceLocation() const = 0;

    // 경로를 만들기 전에 몸체별 내비게이션 기준점을 선택할 기회를 제공한다.
    virtual void PrepareAggressivePathMove(FVector WorldGoal) {}

    // 이동 방향 변경에 필요한 선택적 시각 반응을 처리한다.
    virtual void HandleAggressiveMoveDirectionChanged(const FVector& LocalMoveDirection) {}

    // 경로 이동이 끝났을 때 선택적 완료 통지를 처리한다.
    virtual void HandleAggressivePathMoveCompleted(ECMAggressivePathMoveResult Result) {}
};

/** 실제 다리 임펄스로 움직이는 공격적 AI만 구현하는 다리 구동 계약이다. */
UINTERFACE(MinimalAPI)
class UCMAggressiveLegActuationAgent : public UInterface
{
    GENERATED_BODY()
};

/** 몸체 구성별 다리 수와 다리 조합 활성화를 공통 학습 코드에 제공한다. */
class AI_API ICMAggressiveLegActuationAgent
{
    GENERATED_BODY()

public:
    virtual int32 GetLegCount() const = 0;
    virtual int32 ActivateLegs(const TArray<int32>& LegIndices) = 0;

    // 새 학습 에피소드 전에 몸통별 다리 구동 상태를 선택적으로 초기화한다.
    virtual void ResetLegActuation() {}
};
