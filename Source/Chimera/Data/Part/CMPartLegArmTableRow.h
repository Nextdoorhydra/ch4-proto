#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"

#include "CMPartLegArmTableRow.generated.h"

/** One balance row shared by production Arm and Leg Part actors. */
USTRUCT(BlueprintType)
struct CHIMERA_API FCMPartLegArmTableRow : public FTableRowBase
{
    GENERATED_BODY()

    // RowName is the DataTable key. ID remains a separate unique data ID.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName PartType = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName Species = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxHealth = 0.0f;

    // Arm damage or another concrete Part action's power. Movement uses the
    // separate MovementImpulseMultiplier so combat tuning cannot change speed.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Strength = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float StaminaCost = 0.0f;

    // Leg action lock time and Arm swing time share one sheet column.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ActionDuration = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MovementImpulseMultiplier = 0.0f;

    // Arm-only values. Leg rows explicitly store zero.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttackRange = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttackRadius = 0.0f;
};
