#include "Aggressive/Common/Movement/CMAggressiveKnockbackComponent.h"

#include "Aggressive/Common/Core/CMAggressiveMovementAgent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

UCMAggressiveKnockbackComponent::UCMAggressiveKnockbackComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

// 넉백 방향과 목표 거리를 기록하고 수직 속도를 보존한 초기 속도를 적용한다.
bool UCMAggressiveKnockbackComponent::StartKnockback(FVector Direction, const float DistanceCm)
{
    UPrimitiveComponent* Body = ResolvePhysicsBody();
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Body || !Body->IsSimulatingPhysics() || DistanceCm <= 0.0f)
    {
        return false;
    }

    KnockbackDirection = Direction.GetSafeNormal2D(SMALL_NUMBER, GetOwner()->GetActorForwardVector());
    TargetDistanceCm = DistanceCm;
    StartLocation = Body->GetComponentLocation();
    ElapsedSeconds = 0.0f;
    bActive = true;
    SetComponentTickEnabled(true);

    const float Speed = FMath::Max(TargetDistanceCm / 0.35f, 300.0f);
    const float VerticalVelocity = Body->GetPhysicsLinearVelocity().Z;
    Body->SetPhysicsLinearVelocity(KnockbackDirection * Speed + FVector::UpVector * VerticalVelocity);

    return true;
}

// 이동 거리와 제한시간을 확인하며 남은 거리에 맞춰 넉백 속도를 갱신한다.
void UCMAggressiveKnockbackComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    UPrimitiveComponent* Body = ResolvePhysicsBody();
    if (!bActive || !Body)
    {
        StopKnockback();

        return;
    }

    ElapsedSeconds += DeltaTime;
    const float Travel = FVector::DotProduct(Body->GetComponentLocation() - StartLocation, KnockbackDirection);
    if (Travel >= TargetDistanceCm || ElapsedSeconds >= 0.75f)
    {
        StopKnockback();

        return;
    }

    const float RemainingDistance = TargetDistanceCm - Travel;
    const float Speed = FMath::Max(RemainingDistance / 0.2f, 150.0f);
    const float VerticalVelocity = Body->GetPhysicsLinearVelocity().Z;
    Body->SetPhysicsLinearVelocity(KnockbackDirection * Speed + FVector::UpVector * VerticalVelocity);
}

// 넉백 평면 속도를 제거하고 완료 구독자에게 한 번만 알린다.
void UCMAggressiveKnockbackComponent::StopKnockback()
{
    if (!bActive)
    {
        SetComponentTickEnabled(false);

        return;
    }
    bActive = false;
    SetComponentTickEnabled(false);
    if (UPrimitiveComponent* Body = ResolvePhysicsBody())
    {
        const float VerticalVelocity = Body->GetPhysicsLinearVelocity().Z;
        Body->SetPhysicsLinearVelocity(FVector::UpVector * VerticalVelocity);
    }
    OnKnockbackFinished.Broadcast();
}

UPrimitiveComponent* UCMAggressiveKnockbackComponent::ResolvePhysicsBody() const
{
    const ICMAggressiveMovementAgent* Agent = Cast<ICMAggressiveMovementAgent>(GetOwner());

    return Agent ? Agent->GetAggressiveMovementBody() : nullptr;
}
