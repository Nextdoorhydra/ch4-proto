#include "Aggressive/Common/Movement/CMAggressiveMovementCommandComponent.h"

#include "Aggressive/Common/Core/CMAggressiveMovementAgent.h"
#include "Components/PrimitiveComponent.h"

// 현재 몸통 자세와 목표 위치로 로컬 8방향을 계산한다.
ECMAggressiveMoveDirection CMAggressiveMovementCommand::ResolveGoalDirection(const FVector& BodyLocation, const FQuat& BodyRotation, const FCMAggressiveMovementGoal& Goal)
{
    FVector WorldDirection = Goal.WorldLocation - BodyLocation;
    WorldDirection.Z = 0.0f;
    const float AcceptanceRadius = FMath::Max(Goal.AcceptanceRadius, 0.0f);
    if (WorldDirection.SizeSquared() <= FMath::Square(AcceptanceRadius))
        return ECMAggressiveMoveDirection::None;

    return CMAggressiveDirection::QuantizeWorldDirection8(
        WorldDirection,
        BodyRotation
    );
}

// 다리별 활성 신호에서 임계값 이상인 다리 인덱스를 선택한다.
bool CMAggressiveMovementCommand::SelectActiveLegIndices(const TArray<float>& LegActivationSignals, int32 ExpectedLegCount, float ActivationThreshold, TArray<int32>& OutLegIndices)
{
    OutLegIndices.Reset();
    if (ExpectedLegCount < 0 || LegActivationSignals.Num() != ExpectedLegCount)
        return false;

    const float SafeThreshold = FMath::Clamp(ActivationThreshold, 0.0f, 1.0f);
    OutLegIndices.Reserve(ExpectedLegCount);
    for (int32 LegIndex = 0; LegIndex < ExpectedLegCount; ++LegIndex)
    {
        if (LegActivationSignals[LegIndex] >= SafeThreshold)
            OutLegIndices.Add(LegIndex);
    }
    return true;
}

// Tick을 사용하지 않는 이동 명령 컴포넌트를 생성한다.
UCMAggressiveMovementCommandComponent::UCMAggressiveMovementCommandComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

// 월드 위치와 허용 반경으로 이동 목표를 설정한다.
void UCMAggressiveMovementCommandComponent::SetMovementGoal(FVector WorldLocation, float AcceptanceRadius)
{
    MovementGoal.WorldLocation = WorldLocation;
    MovementGoal.AcceptanceRadius = FMath::Max(AcceptanceRadius, 0.0f);
    bHasMovementGoal = true;
    RefreshGoalDirection();
}

// 현재 이동 목표와 계산된 방향을 초기화한다.
void UCMAggressiveMovementCommandComponent::ClearMovementGoal()
{
    MovementGoal = FCMAggressiveMovementGoal();
    bHasMovementGoal = false;
}

// 현재 몸통 자세를 기준으로 목표의 로컬 8방향을 갱신한다.
ECMAggressiveMoveDirection UCMAggressiveMovementCommandComponent::RefreshGoalDirection()
{
    MovementGoal.LocalDirection = ECMAggressiveMoveDirection::None;
    FVector BodyLocation;
    FQuat BodyRotation;
    if (!bHasMovementGoal || !TryGetBodyState(BodyLocation, BodyRotation))
        return MovementGoal.LocalDirection;

    MovementGoal.LocalDirection =
        CMAggressiveMovementCommand::ResolveGoalDirection(
            BodyLocation,
            BodyRotation,
            MovementGoal
        );
    return MovementGoal.LocalDirection;
}

// 정책의 다리별 활성 신호를 실제 다리 구동으로 전달한다.
int32 UCMAggressiveMovementCommandComponent::ApplyLegActivationSignals(const TArray<float>& LegActivationSignals)
{
    AActor* Owner = GetOwner();
    ICMAggressiveLegActuationAgent* LegAgent = Cast<ICMAggressiveLegActuationAgent>(Owner);
    if (!Owner || !Owner->HasAuthority() || !LegAgent)
    {
        return 0;
    }

    TArray<int32> ActiveLegIndices;
    if (!CMAggressiveMovementCommand::SelectActiveLegIndices(LegActivationSignals, LegAgent->GetLegCount(), LegActivationThreshold, ActiveLegIndices))
        return 0;

    return LegAgent->ActivateLegs(ActiveLegIndices);
}

// 이동 목표의 설정 여부를 반환한다.
bool UCMAggressiveMovementCommandComponent::HasMovementGoal() const
{
    return bHasMovementGoal;
}

// 몸통이 현재 이동 목표의 허용 반경에 도착했는지 반환한다.
bool UCMAggressiveMovementCommandComponent::HasReachedMovementGoal() const
{
    FVector BodyLocation;
    FQuat BodyRotation;
    return bHasMovementGoal && TryGetBodyState(BodyLocation, BodyRotation) && CMAggressiveMovementCommand::ResolveGoalDirection(BodyLocation, BodyRotation, MovementGoal) == ECMAggressiveMoveDirection::None;
}

// 현재 이동 목표 값을 반환한다.
FCMAggressiveMovementGoal UCMAggressiveMovementCommandComponent::GetMovementGoal() const
{
    return MovementGoal;
}

// 소유 Pawn의 몸통 위치와 회전을 가져온다.
bool UCMAggressiveMovementCommandComponent::TryGetBodyState(FVector& OutLocation, FQuat& OutRotation) const
{
    const ICMAggressiveMovementAgent* Agent = Cast<ICMAggressiveMovementAgent>(GetOwner());
    const UPrimitiveComponent* Body = Agent ? Agent->GetAggressiveMovementBody() : nullptr;
    if (!Body)
        return false;

    OutLocation = Body->GetComponentLocation();
    OutRotation = Body->GetComponentQuat();
    return true;
}
