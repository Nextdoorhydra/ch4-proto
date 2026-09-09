#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Texture2D.h"
#include "Ping/CMPingTypes.h"
#include "Ping/CMWorldPing.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMPingDragSelectionTest,
    "Chimera.Multiplayer.Ping.DragSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMPingDragSelectionTest::RunTest(const FString& Parameters)
{
    ECMPingType Type = ECMPingType::GoHere;
    TestFalse(TEXT("Tiny drag remains unselected"),
        CMPing::TrySelectTypeFromDrag(FVector2D(4.0f, 4.0f), Type));
    TestTrue(TEXT("Up selects Go Here"),
        CMPing::TrySelectTypeFromDrag(FVector2D(0.0f, -100.0f), Type));
    TestEqual(TEXT("Up selection"), Type, ECMPingType::GoHere);
    CMPing::TrySelectTypeFromDrag(FVector2D(100.0f, 60.0f), Type);
    TestEqual(TEXT("Down-right selection"), Type, ECMPingType::LookHere);
    CMPing::TrySelectTypeFromDrag(FVector2D(-100.0f, 60.0f), Type);
    TestEqual(TEXT("Down-left selection"), Type, ECMPingType::SwapParts);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMPingLifetimePolicyTest,
    "Chimera.Multiplayer.Ping.LifetimePolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMPingLifetimePolicyTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Ping display duration"),
        CMPing::DisplayDuration, 3.0f);
    TestEqual(TEXT("Maximum active pings"),
        CMPing::MaxActivePings, 6);

    const TArray<float> Ages = { 0.2f, 1.1f, 2.7f, 0.8f, 2.1f, 1.9f };
    TestEqual(TEXT("Oldest ping is selected for eviction"),
        CMPing::FindOldestAgeIndex(Ages), 2);
    TestTrue(TEXT("World ping actor replicates"),
        GetDefault<ACMWorldPing>()->GetIsReplicated());

    const TCHAR* IconPaths[] =
    {
        TEXT("/Game/Chimera/UI/Ping/T_Ping_GoHere.T_Ping_GoHere"),
        TEXT("/Game/Chimera/UI/Ping/T_Ping_LookHere.T_Ping_LookHere"),
        TEXT("/Game/Chimera/UI/Ping/T_Ping_SwapParts.T_Ping_SwapParts")
    };
    for (const TCHAR* IconPath : IconPaths)
    {
        TestNotNull(FString::Printf(TEXT("Ping icon exists: %s"), IconPath),
            LoadObject<UTexture2D>(nullptr, IconPath));
    }
    return true;
}

#endif
