#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CMAggressiveAccelerationMovementComponent.generated.h"

class UPrimitiveComponent;

/** 학습 정책의 평면 가속도 입력을 물리 몸통에 적용하고 최고속도를 제한한다. */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class AI_API UCMAggressiveAccelerationMovementComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMAggressiveAccelerationMovementComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Movement")
    void SetAccelerationInput(FVector WorldPlanarInput);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Movement")
    void StopMovement();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Movement")
    void SetPolicyMovementEnabled(bool bEnabled);

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Movement")
    float GetMaximumSpeed() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Movement")
    FVector GetAccelerationInput() const;

private:
    UPrimitiveComponent* ResolveMovementBody() const;
    void LimitPlanarSpeed(UPrimitiveComponent& Body) const;

    UPROPERTY(EditAnywhere, Category = "Aggressive AI|Movement", meta = (ClampMin = "0.0"))
    float MaximumAcceleration = 1200.0f;

    UPROPERTY(EditAnywhere, Category = "Aggressive AI|Movement", meta = (ClampMin = "0.0"))
    float MaximumSpeed = 300.0f;

    UPROPERTY(VisibleInstanceOnly, Category = "Aggressive AI|Movement")
    FVector AccelerationInput = FVector::ZeroVector;

    bool bPolicyMovementEnabled = true;
};
