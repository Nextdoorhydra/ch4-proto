#include "Common/CMAINavigationRules.h"

bool FCMAINavigationRules::IsWithinProjectionTolerance(const FVector& CurrentLocation, const FVector& ProjectedLocation, const float ToleranceCm)
{
    return FVector::DistSquared2D(CurrentLocation, ProjectedLocation) <= FMath::Square(FMath::Max(ToleranceCm, 0.0f));
}
