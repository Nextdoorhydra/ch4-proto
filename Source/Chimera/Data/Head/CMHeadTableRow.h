#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"

#include "CMHeadTableRow.generated.h"

/** DataForge-compatible gameplay data for one Head Part definition. */
USTRUCT(BlueprintType)
struct CHIMERA_API FCMHeadTableRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ID = NAME_None;

    /** Full horizontal field of view in degrees. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        meta = (ClampMin = "0.0", ClampMax = "360.0"))
    float VisionAngle = 90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        meta = (ClampMin = "0.0"))
    float VisionRange = 1200.0f;

    /** Omnidirectional visible radius around the equipped Head slot. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        meta = (ClampMin = "0.0"))
    float NearVisionRadius = 150.0f;
};
