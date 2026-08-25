#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Aggressive/Common/Core/CMAggressiveAITypes.h"

#include "CMAIFixedLegActuatorComponent.generated.h"

class UPrimitiveComponent;
class USceneComponent;

namespace CMAIFixedLegActuation
{
    /** 몸통 로컬 방향과 크기로 월드 공간 임펄스를 계산한다. */
    AI_API FVector CalculateWorldImpulse(const FTransform& BodyTransform, const FVector& LocalImpulseDirection, float ImpulseMagnitude);

    /** 지정 위치의 임펄스가 만드는 월드 Z축 각운동량을 계산한다. */
    AI_API float CalculateYawAngularImpulse(const FVector& CenterOfMass, const FVector& ApplicationLocation, const FVector& WorldImpulse);
}

/**
 * 고정된 물리량으로 임의 개수의 다리를 구동하는 공격적 AI 전용 컴포넌트다.
 *
 * 다리 메시나 몸통 형태를 소유하지 않고 전달받은 접지점에만 임펄스를 적용한다.
 * 따라서 실험 후 Pawn의 몸통과 다리 구성이 바뀌어도 이 컴포넌트는 유지할 수 있다.
 */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class AI_API UCMAIFixedLegActuatorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMAIFixedLegActuatorComponent();

    /** 현재 Pawn이 보유한 다리 수에 맞춰 쿨다운 상태를 초기화한다. */
    void InitializeLegs(int32 LegCount);

    /** 모든 다리를 즉시 다시 사용할 수 있는 상태로 만든다. */
    void ResetCooldowns();

    /** 현재 월드 시간 기준으로 지정한 다리를 사용할 수 있는지 확인한다. */
    bool IsLegReady(int32 LegIndex) const;

    /** 접지된 다리 하나에 임펄스를 적용한다. */
    bool TryActivateLeg(int32 LegIndex, UPrimitiveComponent* Body, USceneComponent* ContactPoint, const FVector& LocalImpulseDirection, const FCMAIFixedLegActuationSettings& Settings, FCMAIFixedLegActuationResult& OutResult);

    /** 몸통의 수평 속도만 설정된 최대값으로 제한한다. */
    void LimitPlanarSpeed(UPrimitiveComponent* Body, float MaxPlanarSpeed) const;

private:
    bool FindGroundContact(const USceneComponent& ContactPoint, const FCMAIFixedLegActuationSettings& Settings, FHitResult& OutHit) const;

    TArray<double> NextAvailableTimes;
};
