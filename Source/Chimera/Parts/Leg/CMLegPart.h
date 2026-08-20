#pragma once

#include "CoreMinimal.h"
#include "Parts/Core/CMPartActorBase.h"

#include "CMLegPart.generated.h"

/** First production Part that grants the shared Chimera its leg action GA. */
UCLASS(Blueprintable)
class CHIMERA_API ACMLegPart : public ACMPartActorBase
{
    GENERATED_BODY()

public:
    ACMLegPart();

    UFUNCTION(BlueprintPure, Category = "Chimera|Leg")
    float GetStaminaCost() const;

    /** Time during which this specific Leg's GA rejects repeated input. */
    UFUNCTION(BlueprintPure, Category = "Chimera|Leg")
    float GetActionDuration() const;

protected:
    virtual void ApplyPartData(
        const FCMPartLegArmTableRow& PartRow
    ) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Leg",
        meta = (ClampMin = "0.0"))
    float StaminaCost = 10.0f;

    // This is gameplay timing, not animation timing. A later animation may
    // read the same value, but the server remains responsible for the lock.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Leg",
        meta = (ClampMin = "0.01"))
    float ActionDuration = 0.25f;
};
