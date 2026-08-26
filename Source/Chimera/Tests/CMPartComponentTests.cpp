#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "Parts/Arm/CMArmPart.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMPartUsesSkeletalMeshComponentTest,
    "Chimera.Part.Component.SkeletalMesh",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter
)

bool FCMPartUsesSkeletalMeshComponentTest::RunTest(
    const FString& Parameters
)
{
    const ACMArmPart* ArmDefaults = GetDefault<ACMArmPart>();
    const UObject* PartMesh = ArmDefaults
        ? ArmDefaults->GetPartMesh()
        : nullptr;

    TestNotNull(TEXT("Part actor creates the PartMesh subobject"), PartMesh);
    TestTrue(
        TEXT("PartMesh is skeletal and can run an Anim Blueprint"),
        PartMesh && PartMesh->IsA<USkeletalMeshComponent>());
    TestFalse(
        TEXT("PartMesh no longer forces a static mesh component"),
        PartMesh && PartMesh->IsA<UStaticMeshComponent>());
    return true;
}

#endif
