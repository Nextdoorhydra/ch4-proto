#include "Aggressive/Common/Movement/CMAIFixedLegActuatorComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

// 몸통 로컬 입력 방향과 크기를 월드 평면 임펄스로 변환한다.
FVector CMAIFixedLegActuation::CalculateWorldImpulse(const FTransform& BodyTransform, const FVector& LocalImpulseDirection, float ImpulseMagnitude)
{
    const FVector PlanarLocalDirection(
        LocalImpulseDirection.X,
        LocalImpulseDirection.Y,
        0.0f
    );
    if (PlanarLocalDirection.IsNearlyZero() || ImpulseMagnitude <= 0.0f)
        return FVector::ZeroVector;

    FVector WorldDirection = BodyTransform.TransformVectorNoScale(
        PlanarLocalDirection.GetSafeNormal()
    );
    WorldDirection.Z = 0.0f;

    return WorldDirection.GetSafeNormal() * ImpulseMagnitude;
}

// 임펄스 적용 위치가 만드는 월드 Z축 각운동량을 계산한다.
float CMAIFixedLegActuation::CalculateYawAngularImpulse(const FVector& CenterOfMass, const FVector& ApplicationLocation, const FVector& WorldImpulse)
{
    const FVector LeverArm = ApplicationLocation - CenterOfMass;
    return FVector::CrossProduct(LeverArm, WorldImpulse).Z;
}

// Tick을 사용하지 않는 고정 다리 구동 컴포넌트를 생성한다.
UCMAIFixedLegActuatorComponent::UCMAIFixedLegActuatorComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

// 현재 몸통의 다리 수에 맞춰 쿨다운 배열을 초기화한다.
void UCMAIFixedLegActuatorComponent::InitializeLegs(int32 LegCount)
{
    NextAvailableTimes.SetNumZeroed(FMath::Max(LegCount, 0));
}

// 모든 다리의 쿨다운을 즉시 초기화한다.
void UCMAIFixedLegActuatorComponent::ResetCooldowns()
{
    for (double& NextAvailableTime : NextAvailableTimes)
    {
        NextAvailableTime = 0.0;
    }
}

// 현재 월드 시간에 지정한 다리를 사용할 수 있는지 반환한다.
bool UCMAIFixedLegActuatorComponent::IsLegReady(int32 LegIndex) const
{
    const UWorld* World = GetWorld();
    return World && NextAvailableTimes.IsValidIndex(LegIndex) && World->GetTimeSeconds() >= NextAvailableTimes[LegIndex];
}

// 접지와 쿨다운을 검사한 뒤 지정한 다리에 임펄스를 적용한다.
bool UCMAIFixedLegActuatorComponent::TryActivateLeg(int32 LegIndex, UPrimitiveComponent* Body, USceneComponent* ContactPoint, const FVector& LocalImpulseDirection, const FCMAIFixedLegActuationSettings& Settings, FCMAIFixedLegActuationResult& OutResult)
{
    OutResult = FCMAIFixedLegActuationResult();

    const AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!Owner || !Owner->HasAuthority() || !World || !NextAvailableTimes.IsValidIndex(LegIndex) || !Body || !ContactPoint || !Body->IsSimulatingPhysics() || !IsLegReady(LegIndex))
    {
        return false;
    }

    const FVector WorldImpulse =
        CMAIFixedLegActuation::CalculateWorldImpulse(
            Body->GetComponentTransform(),
            LocalImpulseDirection,
            Settings.ImpulseMagnitude
        );
    if (WorldImpulse.IsNearlyZero())
        return false;

    FHitResult GroundHit;
    if (!FindGroundContact(*ContactPoint, Settings, GroundHit))
        return false;

    Body->AddImpulseAtLocation(WorldImpulse, GroundHit.ImpactPoint);
    NextAvailableTimes[LegIndex] = World->GetTimeSeconds()
        + FMath::Max(Settings.CooldownSeconds, 0.0f);

    OutResult.LegIndex = LegIndex;
    OutResult.WorldImpulse = WorldImpulse;
    OutResult.ApplicationLocation = GroundHit.ImpactPoint;
    OutResult.YawAngularImpulse =
        CMAIFixedLegActuation::CalculateYawAngularImpulse(
            Body->GetCenterOfMass(),
            GroundHit.ImpactPoint,
            WorldImpulse
        );
    return true;
}

// 몸통의 수직 속도는 유지하면서 평면 속도만 제한한다.
void UCMAIFixedLegActuatorComponent::LimitPlanarSpeed(UPrimitiveComponent* Body, float MaxPlanarSpeed) const
{
    if (!Body || MaxPlanarSpeed <= 0.0f)
        return;

    FVector Velocity = Body->GetPhysicsLinearVelocity();
    const FVector PlanarVelocity(Velocity.X, Velocity.Y, 0.0f);
    if (PlanarVelocity.SizeSquared() <= FMath::Square(MaxPlanarSpeed))
        return;

    const FVector LimitedPlanarVelocity =
        PlanarVelocity.GetSafeNormal() * MaxPlanarSpeed;
    Velocity.X = LimitedPlanarVelocity.X;
    Velocity.Y = LimitedPlanarVelocity.Y;
    Body->SetPhysicsLinearVelocity(Velocity);
}

// 다리 접지점 아래에서 유효한 지면 접촉을 찾는다.
bool UCMAIFixedLegActuatorComponent::FindGroundContact(const USceneComponent& ContactPoint, const FCMAIFixedLegActuationSettings& Settings, FHitResult& OutHit) const
{
    const UWorld* World = GetWorld();
    const AActor* Owner = GetOwner();
    if (!World || !Owner || Settings.GroundCheckRadius <= 0.0f)
    {
        return false;
    }

    const FVector Start = ContactPoint.GetComponentLocation();
    const FVector End = Start - FVector::UpVector
        * FMath::Max(Settings.GroundContactDistance, 0.0f);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Owner);

    if (!World->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, Settings.GroundTraceChannel, FCollisionShape::MakeSphere(Settings.GroundCheckRadius), QueryParams))
        return false;

    return OutHit.ImpactNormal.Z >= Settings.MinimumGroundNormalZ;
}
