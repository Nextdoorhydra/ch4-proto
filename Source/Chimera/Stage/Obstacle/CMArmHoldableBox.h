#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Parts/Arm/CMArmHoldTarget.h"

#include "CMArmHoldableBox.generated.h"

class ACMArmPart;
class UStaticMeshComponent;

// 디폴트 암의 물리 물체 홀드를 검증하는 복제 상자다.
UCLASS(Blueprintable)
class CHIMERA_API ACMArmHoldableBox
    : public AActor
    , public ICMArmHoldTarget
{
    GENERATED_BODY()

public:
    ACMArmHoldableBox();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    virtual bool QueryArmHold_Implementation(
        ACMArmPart* ArmPart,
        FCMArmHoldSpec& OutSpec
    ) const override;

    virtual bool BeginArmHold_Implementation(
        ACMArmPart* ArmPart
    ) override;

    virtual void EndArmHold_Implementation(
        ACMArmPart* ArmPart
    ) override;

    UFUNCTION(BlueprintPure, Category = "Chimera|Arm Holdable Box")
    bool IsHeld() const { return HoldingArm != nullptr; }

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Chimera|Arm Holdable Box")
    TObjectPtr<UStaticMeshComponent> BoxMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Arm Holdable Box",
        meta = (ClampMin = "1.0"))
    float MassInKg = 40.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Arm Holdable Box")
    int32 ArmHoldPriority = 100;

private:
    UPROPERTY(Replicated, VisibleInstanceOnly)
    TObjectPtr<ACMArmPart> HoldingArm;
};
