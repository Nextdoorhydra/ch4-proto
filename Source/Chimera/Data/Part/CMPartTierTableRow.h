#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"

#include "CMPartTierTableRow.generated.h"

/** Shared tier multipliers applied to a concrete Part data row. */
USTRUCT(BlueprintType)
struct CHIMERA_API FCMPartTierTableRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MovementImpulseMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HealthMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float StrengthMultiplier = 1.0f;
};
