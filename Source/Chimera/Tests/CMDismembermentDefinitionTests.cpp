#include "Misc/AutomationTest.h"

#include "Gore/CMDismembermentDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMDismembermentDefinitionTest,
    "Chimera.Gore.DefaultHumanDefinition",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter
)

bool FCMDismembermentDefinitionTest::RunTest(const FString& Parameters)
{
    const TArray<FCMDismembermentPartDefinition> Parts =
        UCMDismembermentDefinition::MakeDefaultHumanParts();

    TestEqual(TEXT("The human layout defines six parts"), Parts.Num(), 6);

    TSet<ECMBodyPart> UniqueParts;
    TSet<FName> UniqueComponents;
    for (const FCMDismembermentPartDefinition& Part : Parts)
    {
        TestTrue(TEXT("Body part is valid"), Part.BodyPart != ECMBodyPart::None);
        TestFalse(TEXT("Component name is configured"), Part.ComponentName.IsNone());
        TestFalse(TEXT("Detached mesh is configured"), Part.DetachedMesh.IsNull());
        TestFalse(TEXT("Physics asset is configured"),
            Part.DetachedPhysicsAsset.IsNull());
        UniqueParts.Add(Part.BodyPart);
        UniqueComponents.Add(Part.ComponentName);
    }

    TestEqual(TEXT("Every body part enum is unique"), UniqueParts.Num(), 6);
    TestEqual(TEXT("Every component binding is unique"),
        UniqueComponents.Num(), 6);
    return true;
}

#endif
