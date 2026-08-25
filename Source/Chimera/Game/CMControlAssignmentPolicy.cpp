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

    OutAssignments.Reset();
    OutAssignments.SetNum(ExistingAssignments.Num());

    // Four players create eight Segments, therefore each side owns exactly
    // eight slots. Shuffle two Left and two Right ownership tickets, then give
    // every player four random slots exclusively from the selected side.
    const bool bUseFourPlayerSingleSideAssignment =
        AssignedPlayerCount == 4
        && ExistingAssignments.Num() == 4
        && SafeSegmentCount
            == 4 * CMControl::SegmentsPerPlayer;
    if (bUseFourPlayerSingleSideAssignment)
    {
        TArray<FCMPartSlotAddress> LeftPartSlots;
        TArray<FCMPartSlotAddress> RightPartSlots;
        LeftPartSlots.Reserve(SafeSegmentCount);
        RightPartSlots.Reserve(SafeSegmentCount);

        for (int32 SegmentIndex = 0;
            SegmentIndex < SafeSegmentCount;
            ++SegmentIndex)
        {
            FCMPartSlotAddress LeftAddress;
            LeftAddress.SegmentIndex = SegmentIndex;
            LeftAddress.PartSlotIndex = 0;
            LeftPartSlots.Add(LeftAddress);

            FCMPartSlotAddress RightAddress;
            RightAddress.SegmentIndex = SegmentIndex;
            RightAddress.PartSlotIndex = 1;
            RightPartSlots.Add(RightAddress);
        }

        const auto Shuffle = [&RandomStream](auto& Values)
        {
            for (int32 Index = Values.Num() - 1; Index > 0; --Index)
            {
                Values.Swap(Index, RandomStream.RandRange(0, Index));
            }
        };
        Shuffle(LeftPartSlots);
        Shuffle(RightPartSlots);

        // 0=Left, 1=Right. Shuffling makes the side random per player while
        // keeping the total assignment balanced at two players per side.
        TArray<uint8> PlayerSides = { 0, 0, 1, 1 };
        Shuffle(PlayerSides);

        for (int32 PlayerIndex = 0;
            PlayerIndex < AssignedPlayerCount;
            ++PlayerIndex)
        {
            TArray<FCMPartSlotAddress>& SideSlots =
                PlayerSides[PlayerIndex] == 0
                ? LeftPartSlots
                : RightPartSlots;
            while (OutAssignments[PlayerIndex].Num()
                    < CMControl::MaxKeysPerPlayer
                && !SideSlots.IsEmpty())
            {
                OutAssignments[PlayerIndex].Add(
                    SideSlots.Pop(EAllowShrinking::No)
                );
            }
        }
        return;
    }

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
