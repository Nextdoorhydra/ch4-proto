#include "Data/Part/CMPartTier.h"

int32 CMPartTier::ToLevel(const ECMPartTier Tier)
{
    return FMath::Clamp(static_cast<int32>(Tier), 1, 5);
}

ECMPartTier CMPartTier::FromLevel(const int32 Level)
{
    return static_cast<ECMPartTier>(FMath::Clamp(Level, 1, 5));
}

FName CMPartTier::ToRowName(const ECMPartTier Tier)
{
    return FName(*FString::Printf(TEXT("Tier%d"), ToLevel(Tier)));
}

ECMPartTier CMPartTier::FromRowName(const FName RowName)
{
    const FString Value = RowName.ToString();
    if (!Value.StartsWith(TEXT("Tier"), ESearchCase::IgnoreCase))
    {
        return ECMPartTier::Tier1;
    }

    return FromLevel(FCString::Atoi(*Value.RightChop(4)));
}
