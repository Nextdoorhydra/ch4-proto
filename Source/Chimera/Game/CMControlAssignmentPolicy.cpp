#include "CMControlAssignmentPolicy.h"

void FCMControlAssignmentPolicy::Rebalance(
    const TArray<TArray<FCMPartSlotAddress>>& ExistingAssignments,
    int32 ActiveSegmentCount,
    FRandomStream& RandomStream,
    TArray<TArray<FCMPartSlotAddress>>& OutAssignments
)
{
    const int32 AssignedPlayerCount = FMath::Min(
        ExistingAssignments.Num(),
        CMControl::MaxPlayers
    );
    const int32 SafeSegmentCount = FMath::Clamp(
        ActiveSegmentCount,
        0,
        CMControl::MaxSegments
    );

    TArray<FCMPartSlotAddress> AvailablePartSlots;
    for (int32 SegmentIndex = 0;
        SegmentIndex < SafeSegmentCount;
        ++SegmentIndex)
    {
        for (int32 PartSlotIndex = 0;
            PartSlotIndex < CMControl::PartSlotsPerSegment;
            ++PartSlotIndex)
        {
            FCMPartSlotAddress& Address =
                AvailablePartSlots.AddDefaulted_GetRef();
            Address.SegmentIndex = SegmentIndex;
            Address.PartSlotIndex = PartSlotIndex;
        }
    }

    for (int32 Index = AvailablePartSlots.Num() - 1;
        Index > 0;
        --Index)
    {
        AvailablePartSlots.Swap(
            Index,
            RandomStream.RandRange(0, Index)
        );
    }

    OutAssignments.Reset();
    OutAssignments.SetNum(ExistingAssignments.Num());

    for (int32 PlayerIndex = 0;
        PlayerIndex < ExistingAssignments.Num();
        ++PlayerIndex)
    {
        while (PlayerIndex < AssignedPlayerCount
            && OutAssignments[PlayerIndex].Num()
                < CMControl::MaxKeysPerPlayer
            && !AvailablePartSlots.IsEmpty())
        {
            OutAssignments[PlayerIndex].Add(
                AvailablePartSlots.Pop(EAllowShrinking::No)
            );
        }
    }
}
