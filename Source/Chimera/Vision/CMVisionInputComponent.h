#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CMVisionInputComponent.generated.h"

class ACMHeadPartActor;

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

    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Vision|Input",
        meta = (ClampMin = "0.01"))
    float AimUpdateInterval = 0.05f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Vision|Input",
        meta = (ClampMin = "0.0"))
    float MinimumTargetMovement = 5.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Vision|Input")
    bool bShowMouseCursor = true;

private:
    UFUNCTION(Server, Unreliable)
    void ServerUpdateVisionTarget(FVector_NetQuantize100 WorldTarget);

    void GetControlledHeadParts(
        TArray<ACMHeadPartActor*>& OutHeadParts
    ) const;

    FVector LastSentWorldTarget = FVector::ZeroVector;
    bool bHasSentWorldTarget = false;
};
