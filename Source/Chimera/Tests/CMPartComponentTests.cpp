#if WITH_DEV_AUTOMATION_TESTS

#include "Collision/CMCollisionChannels.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "Parts/Arm/CMArmPart.h"
#include "Player/CMChimera.h"
#include "Stage/Obstacle/Component/CMAttackEmitterComponent.h"

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

    const UBoxComponent* PartHurtbox = ArmDefaults
        ? ArmDefaults->GetDamageHurtbox()
        : nullptr;
    TestNotNull(
        TEXT("Part actor creates a simple damage hurtbox"),
        PartHurtbox);
    if (PartHurtbox)
    {
        TestEqual(
            TEXT("Part hurtbox blocks only the weapon trace channel"),
            PartHurtbox->GetCollisionResponseToChannel(
                CMCollision::WeaponTrace),
            ECR_Block);
        TestEqual(
            TEXT("Part hurtbox uses the common hurtbox object channel"),
            PartHurtbox->GetCollisionObjectType(),
            CMCollision::ChimeraHurtbox);
        TestTrue(
            TEXT("Part hurtbox generates obstacle overlap events"),
            PartHurtbox->GetGenerateOverlapEvents());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMChimeraSegmentHurtboxMappingTest,
    "Chimera.Health.SegmentHurtboxMapping",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::EngineFilter
)

bool FCMChimeraSegmentHurtboxMappingTest::RunTest(
    const FString& Parameters
)
{
    const ACMChimera* ChimeraDefaults = GetDefault<ACMChimera>();
    const UBoxComponent* FirstHurtbox = ChimeraDefaults
        ? ChimeraDefaults->GetSegmentHurtbox(0)
        : nullptr;

    TestNotNull(
        TEXT("Chimera creates a hurtbox for its first segment"),
        FirstHurtbox);
    if (FirstHurtbox)
    {
        TestEqual(
            TEXT("Every segment hurtbox uses the authored default extent"),
            FirstHurtbox->GetUnscaledBoxExtent(),
            FVector(50.0f, 40.0f, 37.0f));
        TestEqual(
            TEXT("Segment hurtbox uses the common hurtbox object channel"),
            FirstHurtbox->GetCollisionObjectType(),
            CMCollision::ChimeraHurtbox);
        TestTrue(
            TEXT("Segment hurtbox generates obstacle overlap events"),
            FirstHurtbox->GetGenerateOverlapEvents());
    }
    TestEqual(
        TEXT("Segment hurtbox maps directly to its stable segment index"),
        ChimeraDefaults
            ? ChimeraDefaults->GetSegmentIndexFromHurtbox(FirstHurtbox)
            : INDEX_NONE,
        0);
    TestEqual(
        TEXT("Segment body mesh maps to the same stable segment index"),
        ChimeraDefaults && FirstHurtbox
            ? ChimeraDefaults->GetSegmentIndexFromDamageComponent(
                Cast<UPrimitiveComponent>(FirstHurtbox->GetAttachParent()))
            : INDEX_NONE,
        0);
    TestNull(
        TEXT("Out-of-range segment has no hurtbox"),
        ChimeraDefaults
            ? ChimeraDefaults->GetSegmentHurtbox(CMControl::MaxSegments)
            : nullptr);

    const UCMAttackEmitterComponent* AttackEmitterDefaults =
        GetDefault<UCMAttackEmitterComponent>();
    TestEqual(
        TEXT("Hitscan attacks use the common weapon trace by default"),
        AttackEmitterDefaults->TraceChannel.GetValue(),
        CMCollision::WeaponTrace);
    return true;
}

#endif
