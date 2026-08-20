#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Parts/Core/CMPartActorBase.h"

#include "CMHeadPartActor.generated.h"

class UCMVisionComponent;

/** A Head Part that contributes one cone to the team's shared vision. */
UCLASS(Blueprintable)
class CHIMERA_API ACMHeadPartActor : public ACMPartActorBase
{
    GENERATED_BODY()

public:
    ACMHeadPartActor();

    virtual void OnAttachedToPartSlot_Implementation(
        UCMPartSlotComponent* PartSlot
    ) override;

    virtual void OnDetachedFromPartSlot_Implementation(
        UCMPartSlotComponent* PartSlot
    ) override;

    UFUNCTION(BlueprintPure, Category = "Chimera|Part|Head")
    UCMVisionComponent* GetVisionComponent() const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Chimera|Part|Head")
    TObjectPtr<UCMVisionComponent> VisionComponent;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part|Head")
    FDataTableRowHandle HeadDataRow;

private:
    void ApplyHeadData();
};
