#pragma once

#include "CoreMinimal.h"

#include "CMPartTier.generated.h"

UENUM(BlueprintType)
enum class ECMPartTier : uint8
{
    Invalid = 0,
    Tier1 = 1,
    Tier2 = 2,
    Tier3 = 3,
    Tier4 = 4,
    Tier5 = 5
};

namespace CMPartTier
{
    CHIMERA_API int32 ToLevel(ECMPartTier Tier);
    CHIMERA_API ECMPartTier FromLevel(int32 Level);
    CHIMERA_API FName ToRowName(ECMPartTier Tier);
    CHIMERA_API ECMPartTier FromRowName(FName RowName);
}
