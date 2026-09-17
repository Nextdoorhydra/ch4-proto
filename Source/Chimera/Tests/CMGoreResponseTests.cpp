#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/CMBloodTransferComponent.h"
#include "Gore/CMGoreResponseComponent.h"
#include "Parts/Leg/CMLegPart.h"
#include "Player/CMChimera.h"
#include "Settings/CMBloodSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCMGoreResponseWiringTest,
    "Chimera.Gore.ResponseWiring",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FCMGoreResponseWiringTest::RunTest(const FString& Parameters)
{
    const ACMChimera* Chimera = GetDefault<ACMChimera>();
    const UCMGoreResponseComponent* BodyGore = Chimera
        ? Chimera->FindComponentByClass<UCMGoreResponseComponent>()
        : nullptr;
    TestNotNull(TEXT("Chimera body owns a gore response"), BodyGore);
    if (BodyGore)
    {
        TestTrue(
            TEXT("Chimera body gore response replicates"),
            BodyGore->GetIsReplicated());
        TestEqual(
            TEXT("Chimera body reuses the Sacrifice blood definition"),
            BodyGore->BloodDefinitionId,
            FName(TEXT("Human.Red")));
    }

    const ACMLegPart* Leg = GetDefault<ACMLegPart>();
    const UCMGoreResponseComponent* PartGore = Leg
        ? Leg->FindComponentByClass<UCMGoreResponseComponent>()
        : nullptr;
    TestNotNull(TEXT("Chimera Part owns a gore response"), PartGore);
    if (PartGore)
    {
        TestTrue(
            TEXT("Chimera Part gore response replicates"),
            PartGore->GetIsReplicated());
        TestEqual(
            TEXT("Chimera Part reuses the Sacrifice blood definition"),
            PartGore->BloodDefinitionId,
            FName(TEXT("Human.Red")));
    }

    const UFunction* HitMulticast =
        UCMGoreResponseComponent::StaticClass()->FindFunctionByName(
            TEXT("MulticastSpawnHitEffects"));
    TestNotNull(TEXT("Hit gore multicast is registered"), HitMulticast);
    if (HitMulticast)
    {
        TestTrue(
            TEXT("Hit gore presentation is multicast"),
            HitMulticast->HasAnyFunctionFlags(FUNC_NetMulticast));
        TestFalse(
            TEXT("Frequent hit gore presentation remains unreliable"),
            HitMulticast->HasAnyFunctionFlags(FUNC_NetReliable));
    }

    const UFunction* DestructionMulticast =
        UCMGoreResponseComponent::StaticClass()->FindFunctionByName(
            TEXT("MulticastSpawnDestructionEffects"));
    TestNotNull(
        TEXT("Destruction gore multicast is registered"),
        DestructionMulticast);
    if (DestructionMulticast)
    {
        TestTrue(
            TEXT("Destruction gore presentation is multicast"),
            DestructionMulticast->HasAnyFunctionFlags(FUNC_NetMulticast));
        TestTrue(
            TEXT("One-shot destruction gore presentation is reliable"),
            DestructionMulticast->HasAnyFunctionFlags(FUNC_NetReliable));
    }

    const UCMBloodTransferComponent* BloodTransfer = Leg
        ? Leg->FindComponentByClass<UCMBloodTransferComponent>()
        : nullptr;
    TestNotNull(TEXT("Chimera Leg owns blood-pool transfer"), BloodTransfer);
    if (BloodTransfer)
    {
        TestEqual(
            TEXT("Leg transfer keeps the shared blood definition fallback"),
            BloodTransfer->DefaultBloodDefinitionId,
            FName(TEXT("Human.Red")));
    }

    const UCMBloodSettings* BloodSettings = GetDefault<UCMBloodSettings>();
    TestNotNull(TEXT("Blood settings are available"), BloodSettings);
    if (BloodSettings)
    {
        TestTrue(
            TEXT("Blood decals keep a generous world mark budget"),
            BloodSettings->MaxActiveBloodMarks >= 1024);
        TestTrue(
            TEXT("Blood strokes keep a generous world mark budget"),
            BloodSettings->MaxActiveBloodStrokeMarks >= 768);
        TestTrue(
            TEXT("Blood decal pool is preallocated for frequent impacts"),
            BloodSettings->InitialBloodDecalPoolSize >= 128);
        TestTrue(
            TEXT("Blood strokes can stamp densely without a tiny frame cap"),
            BloodSettings->MaxStrokeStampsPerFrame >= 32);
    }

    return true;
}

#endif
