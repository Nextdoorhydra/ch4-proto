#include "Aggressive/Common/Movement/CMAggressiveAccelerationMovementComponent.h"

#include "Aggressive/Common/Core/CMAggressiveMovementAgent.h"
#include "Components/PrimitiveComponent.h"

UCMAggressiveAccelerationMovementComponent::UCMAggressiveAccelerationMovementComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UCMAggressiveAccelerationMovementComponent::BeginPlay()
{
    Super::BeginPlay();
    SetComponentTickEnabled(GetOwner() && GetOwner()->HasAuthority());
}

// 서버 물리 프레임마다 정책 가속도를 힘으로 적용하고 평면 최고속도를 제한한다.
void UCMAggressiveAccelerationMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UPrimitiveComponent* Body = ResolveMovementBody();
    if (!bPolicyMovementEnabled || !Body || !Body->IsSimulatingPhysics())
        return;

    const FVector SafeInput = AccelerationInput.GetClampedToMaxSize(1.0f);
    if (!SafeInput.IsNearlyZero())
        Body->AddForce(SafeInput * FMath::Max(MaximumAcceleration, 0.0f) * Body->GetMass());
    LimitPlanarSpeed(*Body);
}

// 정책의 월드 평면 가속도 입력을 서버 권한에서 정규화해 저장한다.
void UCMAggressiveAccelerationMovementComponent::SetAccelerationInput(FVector WorldPlanarInput)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
        return;

    WorldPlanarInput.Z = 0.0f;
    AccelerationInput = WorldPlanarInput.GetClampedToMaxSize(1.0f);
}

// 수직 물리는 유지하면서 정책 입력과 몸체의 평면 이동을 즉시 정지한다.
void UCMAggressiveAccelerationMovementComponent::StopMovement()
{
    AccelerationInput = FVector::ZeroVector;
    UPrimitiveComponent* Body = ResolveMovementBody();
    if (!Body)
        return;

    FVector Velocity = Body->GetPhysicsLinearVelocity();
    Velocity.X = 0.0f;
    Velocity.Y = 0.0f;
    Body->SetPhysicsLinearVelocity(Velocity);
}

void UCMAggressiveAccelerationMovementComponent::SetPolicyMovementEnabled(bool bEnabled)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
        return;
    bPolicyMovementEnabled = bEnabled;
    AccelerationInput = FVector::ZeroVector;
}

void UCMAggressiveAccelerationMovementComponent::SetMaximumSpeedMultiplier(const float Multiplier)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
        return;
    MaximumSpeedMultiplier = FMath::Max(Multiplier, 0.0f);
}

float UCMAggressiveAccelerationMovementComponent::GetMaximumSpeed() const
{
    return MaximumSpeed;
}

FVector UCMAggressiveAccelerationMovementComponent::GetAccelerationInput() const
{
    return AccelerationInput;
}

UPrimitiveComponent* UCMAggressiveAccelerationMovementComponent::ResolveMovementBody() const
{
    const ICMAggressiveMovementAgent* Agent = Cast<ICMAggressiveMovementAgent>(GetOwner());

    return Agent ? Agent->GetAggressiveMovementBody() : nullptr;
}

// 수직 속도를 보존한 채 설정 배율을 적용한 평면 최고속도로 제한한다.
void UCMAggressiveAccelerationMovementComponent::LimitPlanarSpeed(UPrimitiveComponent& Body) const
{
    const float SafeMaximumSpeed = FMath::Max(MaximumSpeed, 0.0f) * FMath::Max(MaximumSpeedMultiplier, 0.0f);
    FVector Velocity = Body.GetPhysicsLinearVelocity();
    const FVector PlanarVelocity(Velocity.X, Velocity.Y, 0.0f);
    if (SafeMaximumSpeed <= 0.0f || PlanarVelocity.SizeSquared() <= FMath::Square(SafeMaximumSpeed))
        return;

    const FVector LimitedVelocity = PlanarVelocity.GetSafeNormal() * SafeMaximumSpeed;
    Velocity.X = LimitedVelocity.X;
    Velocity.Y = LimitedVelocity.Y;
    Body.SetPhysicsLinearVelocity(Velocity);
}
