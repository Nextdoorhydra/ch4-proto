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
            PlayerCount * CMControl::SegmentsPerPlayer,
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
            PlayerCount * CMControl::MaxKeysPerPlayer
        );
        TestTrue(
            FString::Printf(
                TEXT("Unique valid PartSlots for %d players"),
                PlayerCount
            ),
            HasUniqueValidPartSlots(
                NewAssignments,
                PlayerCount * CMControl::SegmentsPerPlayer
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
    constexpr int32 ActiveSegmentCount =
        PlayerCount * CMControl::SegmentsPerPlayer;
    TArray<TArray<FCMPartSlotAddress>> FirstExistingAssignments = {
        { MakeAddress(0, 0), MakeAddress(1, 1) },
        { MakeAddress(2, 0) },
        {}
    };
    TArray<TArray<FCMPartSlotAddress>> SecondExistingAssignments = {
        {},
        { MakeAddress(3, 1), MakeAddress(4, 0) },
        { MakeAddress(5, 1) }
    };
    TArray<TArray<FCMPartSlotAddress>> FirstResult;
    TArray<TArray<FCMPartSlotAddress>> SecondResult;
    TArray<TArray<FCMPartSlotAddress>> DifferentSeedResult;
    FRandomStream FirstRandomStream(42);
    FRandomStream SecondRandomStream(42);
    FRandomStream DifferentRandomStream(99);

    FCMControlAssignmentPolicy::Rebalance(
        FirstExistingAssignments,
        ActiveSegmentCount,
        FirstRandomStream,
        FirstResult
    );
    FCMControlAssignmentPolicy::Rebalance(
        SecondExistingAssignments,
        ActiveSegmentCount,
        SecondRandomStream,
        SecondResult
    );
    FCMControlAssignmentPolicy::Rebalance(
        FirstExistingAssignments,
        ActiveSegmentCount,
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
        HasUniqueValidPartSlots(FirstResult, ActiveSegmentCount)
    );

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FChimeraSoloTestControlMappingTest,
    "Chimera.SoloTest.ControlMapping",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter
)

bool FChimeraSoloTestControlMappingTest::RunTest(
    const FString& Parameters)
{
    for (int32 SegmentIndex = 0;
        SegmentIndex < CMControl::SoloTestSegmentCount;
        ++SegmentIndex)
    {
        const FCMPartSlotAddress LeftAddress =
            CMControl::GetSoloTestPartSlotAddress(SegmentIndex);
        TestEqual(
            FString::Printf(TEXT("Left key %d segment"), SegmentIndex),
            LeftAddress.SegmentIndex,
            SegmentIndex);
        TestEqual(
            FString::Printf(TEXT("Left key %d side"), SegmentIndex),
            LeftAddress.PartSlotIndex,
            0);

        const int32 RightKeyIndex =
            SegmentIndex + CMControl::SoloTestSegmentCount;
        const FCMPartSlotAddress RightAddress =
            CMControl::GetSoloTestPartSlotAddress(RightKeyIndex);
        TestEqual(
            FString::Printf(TEXT("Right key %d segment"), SegmentIndex),
            RightAddress.SegmentIndex,
            SegmentIndex);
        TestEqual(
            FString::Printf(TEXT("Right key %d side"), SegmentIndex),
            RightAddress.PartSlotIndex,
            1);
    }

    TestFalse(
        TEXT("Negative solo key index is invalid"),
        CMControl::IsValidPartSlot(
            CMControl::GetSoloTestPartSlotAddress(-1)));
    TestFalse(
        TEXT("Past-end solo key index is invalid"),
        CMControl::IsValidPartSlot(
            CMControl::GetSoloTestPartSlotAddress(
                CMControl::SoloTestKeyCount)));
    return true;
}

#endif
