#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CMAggressiveKnockbackComponent.generated.h"

DECLARE_MULTICAST_DELEGATE(FCMAggressiveKnockbackFinished);

/** Server physics executor; GAS Ability owns the reaction lifetime. */
UCLASS(ClassGroup = (Chimera))
class AI_API UCMAggressiveKnockbackComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMAggressiveKnockbackComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    bool StartKnockback(FVector Direction, float DistanceCm);
    void StopKnockback();
    bool IsKnockbackActive() const
    {
        return bActive;
    }

    FCMAggressiveKnockbackFinished OnKnockbackFinished;

private:
    UPrimitiveComponent* ResolvePhysicsBody() const;

    bool bActive = false;
    FVector StartLocation = FVector::ZeroVector;
    FVector KnockbackDirection = FVector::ForwardVector;
    float TargetDistanceCm = 0.0f;
    float ElapsedSeconds = 0.0f;
};
