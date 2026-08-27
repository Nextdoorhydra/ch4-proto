#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "CMArmHoldTarget.generated.h"

class ACMArmPart;
class UPrimitiveComponent;

USTRUCT(BlueprintType)
struct CHIMERA_API FCMArmHoldSpec
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chimera|Arm|Hold")
    int32 Priority = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chimera|Arm|Hold")
    FVector HoldLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chimera|Arm|Hold")
    FVector HoldNormal = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chimera|Arm|Hold")
    TObjectPtr<UPrimitiveComponent> TargetComponent = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chimera|Arm|Hold")
    bool bUsePhysicsHandle = false;
};

UINTERFACE(BlueprintType)
class CHIMERA_API UCMArmHoldTarget : public UInterface
{
    GENERATED_BODY()
};

/** Target contract shared by buttons, levers, movable props, and cable ends. */
class CHIMERA_API ICMArmHoldTarget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable,
        Category = "Chimera|Arm|Hold")
    bool QueryArmHold(ACMArmPart* ArmPart, FCMArmHoldSpec& OutSpec) const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable,
        Category = "Chimera|Arm|Hold")
    bool BeginArmHold(ACMArmPart* ArmPart);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable,
        Category = "Chimera|Arm|Hold")
    void EndArmHold(ACMArmPart* ArmPart);
};
