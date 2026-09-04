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

    const bool bHasIncompleteAssignment =
        ExistingAssignments.ContainsByPredicate(
            [](const TArray<FCMPartSlotAddress>& Assignment)
            {
                return Assignment.Num() < CMControl::MaxKeysPerPlayer;
            });
    const bool bInitializeSideAssignments =
        AssignedPlayerCount >= 2
        && SafeSegmentCount
            == AssignedPlayerCount * CMControl::SegmentsPerPlayer
        && bHasIncompleteAssignment;
    if (bInitializeSideAssignments)
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

        TArray<int32> PlayerIndices;
        TArray<uint8> PlayerSides;
        for (int32 PlayerIndex = 0;
            PlayerIndex < AssignedPlayerCount;
            ++PlayerIndex)
        {
            PlayerIndices.Add(PlayerIndex);
        }
        for (int32 SidePlayerIndex = 0;
            SidePlayerIndex < AssignedPlayerCount / 2;
            ++SidePlayerIndex)
        {
            PlayerSides.Add(0);
            PlayerSides.Add(1);
        }
        Shuffle(PlayerIndices);
        Shuffle(PlayerSides);
        for (int32 TicketIndex = 0;
            TicketIndex < PlayerSides.Num();
            ++TicketIndex)
        {
            const int32 PlayerIndex = PlayerIndices[TicketIndex];
            TArray<FCMPartSlotAddress>& SideSlots =
                PlayerSides[TicketIndex] == 0
                ? LeftPartSlots
                : RightPartSlots;
            while (OutAssignments[PlayerIndex].Num()
                    < CMControl::MaxKeysPerPlayer
                && !SideSlots.IsEmpty())
            {
                OutAssignments[PlayerIndex].Add(
                    SideSlots.Pop(EAllowShrinking::No));
            }
        }

        if (AssignedPlayerCount % 2 != 0)
        {
            TArray<FCMPartSlotAddress>& MixedAssignment =
                OutAssignments[PlayerIndices.Last()];
            while (!LeftPartSlots.IsEmpty())
            {
                MixedAssignment.Add(
                    LeftPartSlots.Pop(EAllowShrinking::No));
            }
            while (!RightPartSlots.IsEmpty())
            {
                MixedAssignment.Add(
                    RightPartSlots.Pop(EAllowShrinking::No));
            }
        }
        return;
    }

    TSet<FCMPartSlotAddress> ClaimedPartSlots;
    for (int32 PlayerIndex = 0;
        PlayerIndex < AssignedPlayerCount;
        ++PlayerIndex)
    {
        for (const FCMPartSlotAddress& Address
            : ExistingAssignments[PlayerIndex])
        {
            if (OutAssignments[PlayerIndex].Num()
                    >= CMControl::MaxKeysPerPlayer
                || !CMControl::IsValidPartSlot(
                    Address,
                    SafeSegmentCount)
                || ClaimedPartSlots.Contains(Address))
            {
                continue;
            }

            OutAssignments[PlayerIndex].Add(Address);
            ClaimedPartSlots.Add(Address);
        }
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
            FCMPartSlotAddress Address;
            Address.SegmentIndex = SegmentIndex;
            Address.PartSlotIndex = PartSlotIndex;
            if (!ClaimedPartSlots.Contains(Address))
            {
                AvailablePartSlots.Add(Address);
            }
        }
    }

    for (int32 Index = AvailablePartSlots.Num() - 1;
         Index > 0;
         --Index)
    {
        AvailablePartSlots.Swap(
            Index,
            RandomStream.RandRange(0, Index));
    }

    for (int32 PlayerIndex = 0;
        PlayerIndex < AssignedPlayerCount;
        ++PlayerIndex)
    {
        while (OutAssignments[PlayerIndex].Num()
                < CMControl::MaxKeysPerPlayer
            && !AvailablePartSlots.IsEmpty())
        {
            OutAssignments[PlayerIndex].Add(
                AvailablePartSlots.Pop(EAllowShrinking::No)
            );
        }
    }
}
