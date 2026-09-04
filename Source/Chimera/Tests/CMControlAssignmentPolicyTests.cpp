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

    bool IsSingleSideAssignment(
        const TArray<FCMPartSlotAddress>& Assignment)
    {
        if (Assignment.IsEmpty())
        {
            return false;
        }

        const int32 PartSlotIndex = Assignment[0].PartSlotIndex;
        return Assignment.ContainsByPredicate(
            [PartSlotIndex](const FCMPartSlotAddress& Address)
            {
                return Address.PartSlotIndex != PartSlotIndex;
            }) == false;
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
        int32 LeftOnlyPlayerCount = 0;
        int32 RightOnlyPlayerCount = 0;
        int32 MixedPlayerCount = 0;
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
            if (PlayerCount >= 2)
            {
                TSet<int32> OwnedSegments;
                for (const FCMPartSlotAddress& Address
                    : NewAssignments[PlayerIndex])
                {
                    OwnedSegments.Add(Address.SegmentIndex);
                }
                TestEqual(
                    FString::Printf(
                        TEXT("Four distinct owned segments for player %d of %d"),
                        PlayerIndex,
                        PlayerCount),
                    OwnedSegments.Num(),
                    CMControl::MaxKeysPerPlayer);
            }
            TotalAssignedPartSlots +=
                NewAssignments[PlayerIndex].Num();

            int32 LeftSlotCount = 0;
            for (const FCMPartSlotAddress& Address
                : NewAssignments[PlayerIndex])
            {
                LeftSlotCount += Address.PartSlotIndex == 0 ? 1 : 0;
            }
            if (LeftSlotCount == CMControl::MaxKeysPerPlayer)
            {
                ++LeftOnlyPlayerCount;
            }
            else if (LeftSlotCount == 0)
            {
                ++RightOnlyPlayerCount;
            }
            else
            {
                ++MixedPlayerCount;
                TestEqual(
                    TEXT("Odd-player mixed assignment has two left slots"),
                    LeftSlotCount,
                    CMControl::MaxKeysPerPlayer / 2);
            }
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
        if (PlayerCount >= 2)
        {
            TestEqual(
                TEXT("Left-only player count"),
                LeftOnlyPlayerCount,
                PlayerCount / 2);
            TestEqual(
                TEXT("Right-only player count"),
                RightOnlyPlayerCount,
                PlayerCount / 2);
            TestEqual(
                TEXT("Only an odd final player receives mixed sides"),
                MixedPlayerCount,
                PlayerCount % 2);
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FChimeraControlAssignmentPreservationTest,
    "Chimera.Multiplayer.ControlAssignments.Preservation",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter
)

bool FChimeraControlAssignmentPreservationTest::RunTest(
    const FString& Parameters
)
{
    constexpr int32 PlayerCount = 3;
    constexpr int32 ActiveSegmentCount =
        PlayerCount * CMControl::SegmentsPerPlayer;
    TArray<TArray<FCMPartSlotAddress>> ExistingAssignments = {
        {
            MakeAddress(0, 0), MakeAddress(1, 0),
            MakeAddress(2, 0), MakeAddress(3, 0)
        },
        {
            MakeAddress(0, 1), MakeAddress(1, 1),
            MakeAddress(2, 1), MakeAddress(3, 1)
        },
        {
            MakeAddress(4, 0), MakeAddress(5, 0),
            MakeAddress(4, 1), MakeAddress(5, 1)
        }
    };
    TArray<TArray<FCMPartSlotAddress>> Result;
    FRandomStream RandomStream(42);

    FCMControlAssignmentPolicy::Rebalance(
        ExistingAssignments,
        ActiveSegmentCount,
        RandomStream,
        Result
    );

    TestTrue(
        TEXT("Established three-player assignments are preserved"),
        Result == ExistingAssignments
    );

    TArray<TArray<FCMPartSlotAddress>> RepeatedResult;
    FCMControlAssignmentPolicy::Rebalance(
        Result,
        ActiveSegmentCount,
        RandomStream,
        RepeatedResult
    );
    TestTrue(
        TEXT("Respawn-style repeated rebalance changes no assignments"),
        RepeatedResult == Result
    );

    TArray<TArray<FCMPartSlotAddress>> RemainingAssignments = {
        Result[0], Result[2]
    };
    TArray<TArray<FCMPartSlotAddress>> DisconnectResult;
    FCMControlAssignmentPolicy::Rebalance(
        RemainingAssignments,
        ActiveSegmentCount,
        RandomStream,
        DisconnectResult
    );
    TestTrue(
        TEXT("Disconnect-style rebalance preserves remaining players"),
        DisconnectResult == RemainingAssignments
    );

    constexpr int32 FourPlayerSegmentCount =
        4 * CMControl::SegmentsPerPlayer;
    TArray<TArray<FCMPartSlotAddress>> JoiningAssignments = {
        Result[0], Result[1], Result[2], {}
    };
    TArray<TArray<FCMPartSlotAddress>> FourPlayerResult;
    FCMControlAssignmentPolicy::Rebalance(
        JoiningAssignments,
        FourPlayerSegmentCount,
        RandomStream,
        FourPlayerResult
    );
    for (int32 PlayerIndex = 0;
        PlayerIndex < FourPlayerResult.Num();
        ++PlayerIndex)
    {
        TestTrue(
            FString::Printf(
                TEXT("Joining player %d receives one side only"),
                PlayerIndex),
            IsSingleSideAssignment(FourPlayerResult[PlayerIndex])
        );
    }

    TArray<TArray<FCMPartSlotAddress>> StableFourPlayerResult;
    FCMControlAssignmentPolicy::Rebalance(
        FourPlayerResult,
        FourPlayerSegmentCount,
        RandomStream,
        StableFourPlayerResult
    );
    TestTrue(
        TEXT("Established four-player assignments remain unchanged"),
        StableFourPlayerResult == FourPlayerResult
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
