#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Game/CMControlAssignmentPolicy.h"

namespace
{
    bool HasUniqueValidParts(
        const TArray<TArray<ECMControlPart>>& Assignments
    )
    {
        TSet<ECMControlPart> SeenParts;
        for (const TArray<ECMControlPart>& PlayerAssignments
            : Assignments)
        {
            for (ECMControlPart Part : PlayerAssignments)
            {
                if (!CMControl::IsValidPart(Part)
                    || SeenParts.Contains(Part))
                {
                    return false;
                }

                SeenParts.Add(Part);
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

bool FChimeraControlAssignmentCountTest::RunTest(const FString& Parameters)
{
    const TArray<TArray<int32>> ExpectedCounts = {
        { 4 },
        { 4, 4 },
        { 3, 3, 2 },
        { 2, 2, 2, 2 },
        { 2, 2, 2, 1, 1 },
        { 2, 2, 1, 1, 1, 1 },
        { 2, 1, 1, 1, 1, 1, 1 },
        { 1, 1, 1, 1, 1, 1, 1, 1 }
    };

    for (int32 PlayerCount = 1;
        PlayerCount <= CMControl::MaxPlayers;
        ++PlayerCount)
    {
        TArray<TArray<ECMControlPart>> ExistingAssignments;
        ExistingAssignments.SetNum(PlayerCount);
        TArray<TArray<ECMControlPart>> NewAssignments;
        FRandomStream RandomStream(1000 + PlayerCount);

        FCMControlAssignmentPolicy::Rebalance(
            ExistingAssignments,
            RandomStream,
            NewAssignments
        );

        TestEqual(
            FString::Printf(TEXT("Player array count for %d players"), PlayerCount),
            NewAssignments.Num(),
            PlayerCount
        );
        for (int32 PlayerIndex = 0;
            PlayerIndex < PlayerCount;
            ++PlayerIndex)
        {
            TestEqual(
                FString::Printf(
                    TEXT("Assignment count for player %d of %d"),
                    PlayerIndex,
                    PlayerCount
                ),
                NewAssignments[PlayerIndex].Num(),
                ExpectedCounts[PlayerCount - 1][PlayerIndex]
            );
        }

        TestTrue(
            FString::Printf(TEXT("Unique assignments for %d players"), PlayerCount),
            HasUniqueValidParts(NewAssignments)
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
    TArray<TArray<ECMControlPart>> FirstExistingAssignments = {
        {
            ECMControlPart::FirstLeft,
            ECMControlPart::FirstRight,
            ECMControlPart::SecondLeft,
            ECMControlPart::SecondRight
        },
        {
            ECMControlPart::ThirdLeft,
            ECMControlPart::ThirdRight,
            ECMControlPart::FourthLeft,
            ECMControlPart::FourthRight
        },
        {}
    };
    TArray<TArray<ECMControlPart>> SecondExistingAssignments = {
        {},
        {
            ECMControlPart::FourthRight,
            ECMControlPart::ThirdRight
        },
        {
            ECMControlPart::FirstLeft
        }
    };
    TArray<TArray<ECMControlPart>> FirstResult;
    TArray<TArray<ECMControlPart>> SecondResult;
    TArray<TArray<ECMControlPart>> DifferentSeedResult;
    FRandomStream FirstRandomStream(42);
    FRandomStream SecondRandomStream(42);
    FRandomStream DifferentRandomStream(99);

    FCMControlAssignmentPolicy::Rebalance(
        FirstExistingAssignments,
        FirstRandomStream,
        FirstResult
    );
    FCMControlAssignmentPolicy::Rebalance(
        SecondExistingAssignments,
        SecondRandomStream,
        SecondResult
    );
    FCMControlAssignmentPolicy::Rebalance(
        FirstExistingAssignments,
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
        HasUniqueValidParts(FirstResult)
    );

    return true;
}

#endif
