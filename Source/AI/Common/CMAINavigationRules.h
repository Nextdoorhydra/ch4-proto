#pragma once

#include "CoreMinimal.h"

struct AI_API FCMAINavigationRules
{
    static bool IsWithinProjectionTolerance(const FVector& CurrentLocation, const FVector& ProjectedLocation, float ToleranceCm);
};
