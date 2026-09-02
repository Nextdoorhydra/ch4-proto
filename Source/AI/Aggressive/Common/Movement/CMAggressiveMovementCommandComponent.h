#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Aggressive/Common/Core/CMAggressiveAITypes.h"

#include "CMAggressiveMovementCommandComponent.generated.h"

class ICMAggressiveLegActuationAgent;
class ICMAggressiveMovementAgent;

namespace CMAggressiveMovementCommand
{
    /** 현재 몸통 자세를 기준으로 목표가 속한 로컬 8방향을 계산한다. */
    AI_API ECMAggressiveMoveDirection ResolveGoalDirection(const FVector& BodyLocation, const FQuat& BodyRotation, const FCMAggressiveMovementGoal& Goal);

    /** 다리별 활성 신호를 실제로 구동할 다리 인덱스 배열로 변환한다. */
    AI_API bool SelectActiveLegIndices(const TArray<float>& LegActivationSignals, int32 ExpectedLegCount, float ActivationThreshold, TArray<int32>& OutLegIndices);
} // namespace CMAggressiveMovementCommand

/**
 * 코드가 지정한 이동 목표와 학습 정책의 다리 행동을 연결한다.
 *
 * 방향별 다리 조합은 고정하지 않는다. 학습 정책은 각 다리의 활성 신호를
 * 독립적으로 출력하며 이 컴포넌트는 정해진 판단 시점에만 결과를 적용한다.
 */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class AI_API UCMAggressiveMovementCommandComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMAggressiveMovementCommandComponent();

    virtual void BeginPlay() override;

    /** 월드 위치와 도착 허용 반경으로 현재 이동 목표를 설정한다. */
    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Movement")
    void SetMovementGoal(FVector WorldLocation, float AcceptanceRadius = 0.0f);

    /** 현재 이동 목표를 제거한다. */
    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Movement")
    void ClearMovementGoal();

    /** 현재 몸통 자세를 기준으로 목표의 로컬 8방향을 다시 계산한다. */
    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Movement")
    ECMAggressiveMoveDirection RefreshGoalDirection();

    /** 머신러닝이 출력한 다리별 활성 신호를 물리 구동기에 전달한다. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Movement")
    int32 ApplyLegActivationSignals(const TArray<float>& LegActivationSignals);

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Movement")
    bool HasMovementGoal() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Movement")
    bool HasReachedMovementGoal() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Movement")
    FCMAggressiveMovementGoal GetMovementGoal() const;

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LegActivationThreshold = 0.5f;

private:
    bool TryGetBodyState(FVector& OutLocation, FQuat& OutRotation) const;

    UPROPERTY(VisibleInstanceOnly, Category = "Aggressive AI|Movement")
    FCMAggressiveMovementGoal MovementGoal;

    UPROPERTY(VisibleInstanceOnly, Category = "Aggressive AI|Movement")
    bool bHasMovementGoal = false;

    ICMAggressiveLegActuationAgent* LegActuationAgent = nullptr;
    ICMAggressiveMovementAgent* CachedMovementAgent = nullptr;
};
