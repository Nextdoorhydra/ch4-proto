#pragma once

#include "CoreMinimal.h"
#include "Parts/Arm/CMArmHoldTarget.h"
#include "Stage/Checkpoint/CMCheckpointResettable.h"
#include "Stage/Device/CMStageDeviceBase.h"
#include "CMGrabRailActor.generated.h"

class USplineComponent;
class UBoxComponent;
class USphereComponent;
class UStaticMeshComponent;
class UCMRailMovementComponent;
class UCMInteractionHighlightComponent;

// Generic grab-and-slide prop. Its rail/root stays still; only RailBody and its children move.
UCLASS(Blueprintable)
class CHIMERA_API ACMGrabRailActor
    : public ACMStageDeviceBase
    , public ICMArmHoldTarget
    , public ICMCheckpointResettable
{
    GENERATED_BODY()
public:
    ACMGrabRailActor();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual bool QueryArmHold_Implementation(ACMArmPart* Arm, FCMArmHoldSpec& OutSpec) const override;
    virtual bool BeginArmHold_Implementation(ACMArmPart* Arm) override;
    virtual void EndArmHold_Implementation(ACMArmPart* Arm) override;
    virtual void ResetForCheckpoint() override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void HandleElementActiveChanged_Implementation(bool bIsActive) override;
    virtual void HandleElementReset_Implementation() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<USplineComponent> Rail;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<UBoxComponent> RailBody;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<UStaticMeshComponent> MovingMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<USphereComponent> GrabHandle;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<UCMRailMovementComponent> RailMovement;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<UCMInteractionHighlightComponent> InteractionHighlight;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Interaction Hint", meta = (ClampMin = "0.02", Units = "s"))
    float InteractionHintRefreshInterval = 0.1f;

private:
    void RefreshInteractionHighlight();

    FTimerHandle InteractionHintTimerHandle;
};
