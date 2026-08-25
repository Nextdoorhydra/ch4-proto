#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CMVisionInputComponent.generated.h"

class ACMHeadPartActor;
class UCMVisionComponent;

/**
 * Player-owned cursor input bridge for shared Head vision.
 * Add this component to BP_CMPlayerController so its server RPC has ownership.
 */
UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
class CHIMERA_API UCMVisionInputComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMVisionInputComponent();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Vision|Input",
        meta = (ClampMin = "0.01",
            ToolTip = "Minimum interval between aim RPCs. Local prediction still updates every frame."))
    float AimUpdateInterval = 0.05f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Vision|Input",
        meta = (ClampMin = "0.1",
            ToolTip = "Resends unchanged aim so a lost final unreliable RPC is repaired."))
    float AimHeartbeatInterval = 0.25f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Vision|Input",
        meta = (ClampMin = "0.0"))
    float MinimumTargetMovement = 5.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Vision|Input")
    bool bShowMouseCursor = true;

private:
    UFUNCTION(Server, Unreliable)
    void ServerUpdateVisionTarget(
        FVector_NetQuantize100 WorldTarget,
        float AimRotationDegrees
    );

    void GetControlledHeadParts(
        TArray<ACMHeadPartActor*>& OutHeadParts
    ) const;

    void ReplaceLocalAimPredictions(
        const TSet<UCMVisionComponent*>& CurrentPredictions
    );
    void ClearLocalAimPredictions();

    FVector LastSentWorldTarget = FVector::ZeroVector;
    float TimeSinceLastAimSend = 0.0f;
    float LocalAimRotationDegrees = 0.0f;
    float LastLocalAimAngleDegrees = 0.0f;
    float LastSentAimRotationDegrees = 0.0f;
    bool bHasSentWorldTarget = false;
    bool bHasLocalAimRotation = false;
    TSet<TWeakObjectPtr<UCMVisionComponent>> LocallyPredictedVisions;
};
