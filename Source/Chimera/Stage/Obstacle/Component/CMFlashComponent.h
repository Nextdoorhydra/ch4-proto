#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CMFlashComponent.generated.h"

UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
class CHIMERA_API UCMFlashComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMFlashComponent();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Obstacle|Flash")
    void TriggerFlash();

    UFUNCTION(BlueprintCallable, Category = "Chimera|Obstacle|Flash")
    void SetFlashEnabled(bool bEnabled);

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Obstacle|Flash",
        meta = (ClampMin = "0.0", Units = "cm"))
    float FlashRange = 1500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Obstacle|Flash",
        meta = (ClampMin = "0.01", Units = "s"))
    float BlindDuration = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Obstacle|Flash",
        meta = (ClampMin = "0.0", Units = "s"))
    float BlindActivationDelay = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Obstacle|Flash",
        meta = (ClampMin = "0.01", Units = "s"))
    float BlindRecoveryDuration = 0.75f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Obstacle|Flash")
    bool bRequireLineOfSight = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Obstacle|Flash",
        meta = (EditCondition = "bRequireLineOfSight"))
    TEnumAsByte<ECollisionChannel> LineOfSightTraceChannel = ECC_Visibility;

private:
    void DetonateFlash();
    bool HasLineOfSight(const class UCMVisionComponent& VisionSource) const;

    bool bFlashEnabled = true;
    bool bFlashPending = false;
    FTimerHandle DetonationTimerHandle;
};
