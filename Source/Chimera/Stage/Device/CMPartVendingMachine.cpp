#include "Stage/Device/CMPartVendingMachine.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Parts/Arm/CMArmPart.h"
#include "Parts/Core/CMPartActorBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraPartVendingMachine, Log, All);

ACMPartVendingMachine::ACMPartVendingMachine()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    MachineMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("MachineMesh"));
    MachineMesh->SetupAttachment(SceneRoot);
    MachineMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

    HitVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("HitVolume"));
    HitVolume->SetupAttachment(SceneRoot);
    HitVolume->SetBoxExtent(FVector(75.0f, 75.0f, 100.0f));
    HitVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    HitVolume->SetCollisionObjectType(ECC_WorldDynamic);
    HitVolume->SetCollisionResponseToAllChannels(ECR_Overlap);
    HitVolume->SetGenerateOverlapEvents(true);
    HitVolume->SetCanEverAffectNavigation(false);

    DispensePoint = CreateDefaultSubobject<USceneComponent>(
        TEXT("DispensePoint"));
    DispensePoint->SetupAttachment(SceneRoot);
    DispensePoint->SetRelativeLocation(FVector(100.0f, 0.0f, 50.0f));
}

bool ACMPartVendingMachine::ReceiveCombatHit_Implementation(
    const FCMCombatHitRequest& Request)
{
    if (!HasAuthority()
        || !IsValid(Cast<ACMArmPart>(Request.SourcePart))
        || !Request.AttackId.IsValid())
    {
        return false;
    }
    if (Request.AttackId == LastAcceptedAttackId)
    {
        return true;
    }

    ACMPartActorBase* DispensedPart = DispensePart();
    if (!DispensedPart)
    {
        return false;
    }

    LastAcceptedAttackId = Request.AttackId;
    OnPartDispensed(DispensedPart);
    return true;
}

ACMPartActorBase* ACMPartVendingMachine::DispensePart()
{
    if (!HasAuthority() || !PartClass || !DispensePoint)
    {
        UE_LOG(LogChimeraPartVendingMachine, Warning,
            TEXT("Part vending machine has no valid output. Machine=%s PartClass=%s"),
            *GetName(), *GetNameSafe(PartClass.Get()));
        return nullptr;
    }

    ACMPartActorBase* Part = ACMPartActorBase::SpawnPartFromDataRows(
        this,
        PartClass,
        PartRowName,
        TierRowName,
        DispensePoint->GetComponentTransform(),
        nullptr);
    if (!Part)
    {
        return nullptr;
    }

    if (USkeletalMeshComponent* PartMesh = Part->GetPartMesh())
    {
        const FVector EjectVelocity = GetActorTransform()
            .TransformVectorNoScale(LocalEjectVelocity);
        PartMesh->AddImpulse(EjectVelocity, NAME_None, true);
    }

    UE_LOG(LogChimeraPartVendingMachine, Display,
        TEXT("Part dispensed. Machine=%s Part=%s Class=%s"),
        *GetName(), *GetNameSafe(Part), *GetNameSafe(PartClass.Get()));
    return Part;
}
