#include "CMControlAssignmentPolicy.h"

void FCMControlAssignmentPolicy::Rebalance(
    const TArray<TArray<ECMControlPart>>& ExistingAssignments,
    FRandomStream& RandomStream,
    TArray<TArray<ECMControlPart>>& OutAssignments
)
{
    const int32 AssignedPlayerCount = FMath::Min(
        ExistingAssignments.Num(),
        CMControl::MaxPlayers
    );
    const int32 TotalAssignedParts = FMath::Min(
        CMControl::MaxControlParts,
        AssignedPlayerCount * CMControl::MaxKeysPerPlayer
    );

    TArray<int32> TargetCounts;
    TargetCounts.Init(0, ExistingAssignments.Num());
    if (AssignedPlayerCount > 0)
    {
        const int32 BaseCount = TotalAssignedParts / AssignedPlayerCount;
        const int32 Remainder = TotalAssignedParts % AssignedPlayerCount;
        for (int32 PlayerIndex = 0;
            PlayerIndex < AssignedPlayerCount;
            ++PlayerIndex)
        {
            TargetCounts[PlayerIndex] =
                BaseCount + (PlayerIndex < Remainder ? 1 : 0);
        }
    }

    TArray<ECMControlPart> AvailableParts;
    for (int32 PartIndex = 0;
        PartIndex < CMControl::MaxControlParts;
        ++PartIndex)
    {
        AvailableParts.Add(static_cast<ECMControlPart>(PartIndex));
    }

    for (int32 Index = AvailableParts.Num() - 1; Index > 0; --Index)
    {
        AvailableParts.Swap(Index, RandomStream.RandRange(0, Index));
    }

    OutAssignments.Reset();
    OutAssignments.SetNum(ExistingAssignments.Num());

    for (int32 PlayerIndex = 0;
        PlayerIndex < ExistingAssignments.Num();
        ++PlayerIndex)
    {
        while (OutAssignments[PlayerIndex].Num()
                < TargetCounts[PlayerIndex]
            && !AvailableParts.IsEmpty())
        {
            OutAssignments[PlayerIndex].Add(
                AvailableParts.Pop(EAllowShrinking::No)
            );
        }
    }
}
