#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Chimera/Game/ChimeraControlAssignmentPolicy.h"

namespace
{
    bool HasUniqueValidParts(
        const TArray<TArray<EChimeraControlPart>>& Assignments
    )
    {
        TSet<EChimeraControlPart> SeenParts;
        for (const TArray<EChimeraControlPart>& PlayerAssignments
            : Assignments)
        {
            for (EChimeraControlPart Part : PlayerAssignments)
            {
                if (!ChimeraControl::IsValidPart(Part)
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
        PlayerCount <= ChimeraControl::MaxPlayers;
        ++PlayerCount)
    {
        TArray<TArray<EChimeraControlPart>> ExistingAssignments;
        ExistingAssignments.SetNum(PlayerCount);
        TArray<TArray<EChimeraControlPart>> NewAssignments;
        FRandomStream RandomStream(1000 + PlayerCount);

        FChimeraControlAssignmentPolicy::Rebalance(
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
    TArray<TArray<EChimeraControlPart>> FirstExistingAssignments = {
        {
            EChimeraControlPart::FirstLeft,
            EChimeraControlPart::FirstRight,
            EChimeraControlPart::SecondLeft,
            EChimeraControlPart::SecondRight
        },
        {
            EChimeraControlPart::ThirdLeft,
            EChimeraControlPart::ThirdRight,
            EChimeraControlPart::FourthLeft,
            EChimeraControlPart::FourthRight
        },
        {}
    };
    TArray<TArray<EChimeraControlPart>> SecondExistingAssignments = {
        {},
        {
            EChimeraControlPart::FourthRight,
            EChimeraControlPart::ThirdRight
        },
        {
            EChimeraControlPart::FirstLeft
        }
    };
    TArray<TArray<EChimeraControlPart>> FirstResult;
    TArray<TArray<EChimeraControlPart>> SecondResult;
    TArray<TArray<EChimeraControlPart>> DifferentSeedResult;
    FRandomStream FirstRandomStream(42);
    FRandomStream SecondRandomStream(42);
    FRandomStream DifferentRandomStream(99);

    FChimeraControlAssignmentPolicy::Rebalance(
        FirstExistingAssignments,
        FirstRandomStream,
        FirstResult
    );
    FChimeraControlAssignmentPolicy::Rebalance(
        SecondExistingAssignments,
        SecondRandomStream,
        SecondResult
    );
    FChimeraControlAssignmentPolicy::Rebalance(
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
