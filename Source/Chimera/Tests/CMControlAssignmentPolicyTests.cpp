#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Game/CMControlAssignmentPolicy.h"

namespace
{
    FCMPartSlotAddress MakeAddress(
        int32 SegmentIndex,
        int32 PartSlotIndex
    )
    {
        FCMPartSlotAddress Address;
        Address.SegmentIndex = SegmentIndex;
        Address.PartSlotIndex = PartSlotIndex;
        return Address;
    }

    bool HasUniqueValidPartSlots(
        const TArray<TArray<FCMPartSlotAddress>>& Assignments,
        int32 ActiveSegmentCount
    )
    {
        TSet<FCMPartSlotAddress> SeenPartSlots;
        for (const TArray<FCMPartSlotAddress>& PlayerAssignments
            : Assignments)
        {
            for (const FCMPartSlotAddress& Address
                : PlayerAssignments)
            {
                if (!CMControl::IsValidPartSlot(
                        Address,
                        ActiveSegmentCount)
                    || SeenPartSlots.Contains(Address))
                {
                    return false;
                }

                SeenPartSlots.Add(Address);
            }
        }

        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FChimeraControlAssignmentCountTest,
    "Chimera.Multiplayer.ControlAssignments.PlayerCounts",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter
)

bool FChimeraControlAssignmentCountTest::RunTest(
    const FString& Parameters
)
{
    for (int32 PlayerCount = 1;
        PlayerCount <= CMControl::MaxPlayers;
        ++PlayerCount)
    {
        TArray<TArray<FCMPartSlotAddress>> ExistingAssignments;
        ExistingAssignments.SetNum(PlayerCount);
        TArray<TArray<FCMPartSlotAddress>> NewAssignments;
        FRandomStream RandomStream(1000 + PlayerCount);

        FCMControlAssignmentPolicy::Rebalance(
            ExistingAssignments,
            PlayerCount,
            RandomStream,
            NewAssignments
        );

        TestEqual(
            FString::Printf(
                TEXT("Player array count for %d players"),
                PlayerCount
            ),
            NewAssignments.Num(),
            PlayerCount
        );

        int32 TotalAssignedPartSlots = 0;
        for (int32 PlayerIndex = 0;
            PlayerIndex < PlayerCount;
            ++PlayerIndex)
        {
            TestEqual(
                FString::Printf(
                    TEXT("Four controls for player %d of %d"),
                    PlayerIndex,
                    PlayerCount
                ),
                NewAssignments[PlayerIndex].Num(),
                CMControl::MaxKeysPerPlayer
            );
            TotalAssignedPartSlots +=
                NewAssignments[PlayerIndex].Num();
        }

        TestEqual(
            FString::Printf(
                TEXT("All active PartSlots assigned for %d players"),
                PlayerCount
            ),
            TotalAssignedPartSlots,
            PlayerCount * CMControl::PartSlotsPerSegment
        );
        TestTrue(
            FString::Printf(
                TEXT("Unique valid PartSlots for %d players"),
                PlayerCount
            ),
            HasUniqueValidPartSlots(
                NewAssignments,
                PlayerCount
            )
        );
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FChimeraControlAssignmentFullShuffleTest,
    "Chimera.Multiplayer.ControlAssignments.FullShuffle",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter
)

bool FChimeraControlAssignmentFullShuffleTest::RunTest(
    const FString& Parameters
)
{
    constexpr int32 PlayerCount = 3;
    TArray<TArray<FCMPartSlotAddress>> FirstExistingAssignments = {
        {
            MakeAddress(0, 0),
            MakeAddress(1, 1),
            MakeAddress(2, 2),
            MakeAddress(0, 3)
        },
        { MakeAddress(1, 0) },
        {}
    };
    TArray<TArray<FCMPartSlotAddress>> SecondExistingAssignments = {
        {},
        { MakeAddress(2, 3), MakeAddress(0, 1) },
        { MakeAddress(1, 2) }
    };
    TArray<TArray<FCMPartSlotAddress>> FirstResult;
    TArray<TArray<FCMPartSlotAddress>> SecondResult;
    TArray<TArray<FCMPartSlotAddress>> DifferentSeedResult;
    FRandomStream FirstRandomStream(42);
    FRandomStream SecondRandomStream(42);
    FRandomStream DifferentRandomStream(99);

    FCMControlAssignmentPolicy::Rebalance(
        FirstExistingAssignments,
        PlayerCount,
        FirstRandomStream,
        FirstResult
    );
    FCMControlAssignmentPolicy::Rebalance(
        SecondExistingAssignments,
        PlayerCount,
        SecondRandomStream,
        SecondResult
    );
    FCMControlAssignmentPolicy::Rebalance(
        FirstExistingAssignments,
        PlayerCount,
        DifferentRandomStream,
        DifferentSeedResult
    );

    TestTrue(
        TEXT("Same seed ignores previous ownership"),
        FirstResult == SecondResult
    );
    TestTrue(
        TEXT("Different seeds produce different assignments"),
        FirstResult != DifferentSeedResult
    );
    TestTrue(
        TEXT("Fully shuffled assignments remain unique"),
        HasUniqueValidPartSlots(FirstResult, PlayerCount)
    );

    return true;
}

#endif
