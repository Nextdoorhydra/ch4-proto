#include "Ping/CMPingTypes.h"

bool CMPing::TrySelectTypeFromDrag(
    const FVector2D& Drag,
    ECMPingType& OutType)
{
    if (Drag.SizeSquared() < FMath::Square(SelectionDeadZone))
    {
        return false;
    }

    const FVector2D Direction = Drag.GetSafeNormal();
    const FVector2D SectorDirections[] =
    {
        FVector2D(0.0f, -1.0f),
        FVector2D(0.866f, 0.5f),
        FVector2D(-0.866f, 0.5f)
    };
    const ECMPingType SectorTypes[] =
    {
        ECMPingType::GoHere,
        ECMPingType::LookHere,
        ECMPingType::SwapParts
    };

    int32 BestIndex = 0;
    float BestDot = FVector2D::DotProduct(Direction, SectorDirections[0]);
    for (int32 Index = 1; Index < UE_ARRAY_COUNT(SectorDirections); ++Index)
    {
        const float Dot = FVector2D::DotProduct(
            Direction, SectorDirections[Index]);
        if (Dot > BestDot)
        {
            BestDot = Dot;
            BestIndex = Index;
        }
    }

    OutType = SectorTypes[BestIndex];
    return true;
}

FLinearColor CMPing::GetTypeColor(ECMPingType Type)
{
    switch (Type)
    {
    case ECMPingType::GoHere:
        return FLinearColor(0.16f, 0.95f, 0.22f, 1.0f);
    case ECMPingType::LookHere:
        return FLinearColor(0.08f, 0.48f, 1.0f, 1.0f);
    case ECMPingType::SwapParts:
        return FLinearColor(1.0f, 0.32f, 0.04f, 1.0f);
    default:
        return FLinearColor::White;
    }
}

int32 CMPing::FindOldestAgeIndex(TConstArrayView<float> Ages)
{
    if (Ages.IsEmpty())
    {
        return INDEX_NONE;
    }

    int32 OldestIndex = 0;
    for (int32 Index = 1; Index < Ages.Num(); ++Index)
    {
        if (Ages[Index] > Ages[OldestIndex])
        {
            OldestIndex = Index;
        }
    }
    return OldestIndex;
}
