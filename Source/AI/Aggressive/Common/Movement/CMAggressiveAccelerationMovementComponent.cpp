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

void UCMAggressiveAccelerationMovementComponent::SetAccelerationInput(FVector WorldPlanarInput)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
        return;

    WorldPlanarInput.Z = 0.0f;
    AccelerationInput = WorldPlanarInput.GetClampedToMaxSize(1.0f);
}

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

void UCMAggressiveAccelerationMovementComponent::LimitPlanarSpeed(UPrimitiveComponent& Body) const
{
    const float SafeMaximumSpeed = FMath::Max(MaximumSpeed, 0.0f);
    FVector Velocity = Body.GetPhysicsLinearVelocity();
    const FVector PlanarVelocity(Velocity.X, Velocity.Y, 0.0f);
    if (SafeMaximumSpeed <= 0.0f || PlanarVelocity.SizeSquared() <= FMath::Square(SafeMaximumSpeed))
        return;

    const FVector LimitedVelocity = PlanarVelocity.GetSafeNormal() * SafeMaximumSpeed;
    Velocity.X = LimitedVelocity.X;
    Velocity.Y = LimitedVelocity.Y;
    Body.SetPhysicsLinearVelocity(Velocity);
}
