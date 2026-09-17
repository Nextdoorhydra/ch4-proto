#include "Network/CMRoomId.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMRoomIdGenerationTest,
    "Chimera.UI.RoomId.Generation",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter
)

bool FCMRoomIdGenerationTest::RunTest(const FString&)
{
    for (int32 Index = 0; Index < 256; ++Index)
    {
        const FString Generated = CMRoomId::Generate();
        FString Normalized;
        TestEqual(TEXT("Room ID has six characters"), Generated.Len(), 6);
        TestTrue(
            TEXT("Generated room ID is valid"),
            CMRoomId::NormalizeAndValidate(Generated, Normalized)
        );
        TestEqual(TEXT("Generated room ID is already normalized"), Normalized, Generated);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMRoomIdValidationTest,
    "Chimera.UI.RoomId.Validation",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter
)

bool FCMRoomIdValidationTest::RunTest(const FString&)
{
    FString Normalized;
    TestTrue(
        TEXT("Lowercase input is normalized"),
        CMRoomId::NormalizeAndValidate(TEXT(" ab12cd "), Normalized)
    );
    TestEqual(TEXT("Normalized room ID"), Normalized, TEXT("AB12CD"));
    TestFalse(
        TEXT("Letters-only input is rejected"),
        CMRoomId::NormalizeAndValidate(TEXT("ABCDEF"), Normalized)
    );
    TestFalse(
        TEXT("Digits-only input is rejected"),
        CMRoomId::NormalizeAndValidate(TEXT("123456"), Normalized)
    );
    TestFalse(
        TEXT("Punctuation is rejected"),
        CMRoomId::NormalizeAndValidate(TEXT("AB-123"), Normalized)
    );
    return true;
}

#endif
