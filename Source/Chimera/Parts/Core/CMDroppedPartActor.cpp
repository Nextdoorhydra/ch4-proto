#include "Parts/Core/CMDroppedPartActor.h"

#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Parts/Tentacle/CMTentacleSegmentActor.h"
#include "Player/CMChimera.h"
#include "Player/CMPartSlotComponent.h"

ACMDroppedPartActor::ACMDroppedPartActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    PartMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PartMesh"));
    SetRootComponent(PartMesh);
    PartMesh->SetIsReplicated(true);
    PartMesh->SetGenerateOverlapEvents(true);
    Tags.AddUnique(
        ACMTentacleSegmentActor::TentacleInteractiveActorTag);
}

void ACMDroppedPartActor::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACMDroppedPartActor, BodyPart);
    DOREPLIFETIME(ACMDroppedPartActor, UsablePartClass);
    DOREPLIFETIME(ACMDroppedPartActor, DroppedMesh);
    DOREPLIFETIME(ACMDroppedPartActor, DroppedPhysicsAsset);
    DOREPLIFETIME(ACMDroppedPartActor, DroppedCollisionProfile);
    DOREPLIFETIME(ACMDroppedPartActor, bTentaclePulled);
}

void ACMDroppedPartActor::InitializeDroppedPart(
    const ECMBodyPart InBodyPart,
    TSubclassOf<ACMPartActorBase> InUsablePartClass,
    USkeletalMesh* InMesh,
    UPhysicsAsset* InPhysicsAsset,
    const FName CollisionProfile,
    const FVector Impulse
)
{
    if (!HasAuthority() || !PartMesh)
    {
        return;
    }

    BodyPart = InBodyPart;
    UsablePartClass = InUsablePartClass;
    DroppedMesh = InMesh;
    DroppedPhysicsAsset = InPhysicsAsset;
    DroppedCollisionProfile = CollisionProfile;
    ApplyVisualDefinition();
    if (!Impulse.IsNearlyZero())
    {
        PartMesh->AddImpulse(Impulse);
    }
    ForceNetUpdate();
}

void ACMDroppedPartActor::OnRep_VisualDefinition()
{
    ApplyVisualDefinition();
}

void ACMDroppedPartActor::ApplyVisualDefinition()
{
    if (!PartMesh || !DroppedMesh || !DroppedPhysicsAsset)
    {
        return;
    }
    PartMesh->SetSkeletalMeshAsset(DroppedMesh);
    PartMesh->SetPhysicsAsset(DroppedPhysicsAsset, false);
    PartMesh->SetCollisionProfileName(DroppedCollisionProfile);
    PartMesh->SetCollisionEnabled(
        bTentaclePulled
            ? ECollisionEnabled::NoCollision
            : ECollisionEnabled::QueryAndPhysics);
    PartMesh->SetAllBodiesSimulatePhysics(!bTentaclePulled);
    PartMesh->SetSimulatePhysics(!bTentaclePulled);
    if (!bTentaclePulled)
    {
        PartMesh->WakeAllRigidBodies();
    }
}

void ACMDroppedPartActor::OnRep_TentaclePulled()
{
    ApplyVisualDefinition();
}

bool ACMDroppedPartActor::TryReserveForTentacle(AActor* Requester)
{
    if (!HasAuthority() || !IsValid(Requester) || bConsumed)
    {
        return false;
    }
    if (TentacleReservationOwner.IsValid()
        && TentacleReservationOwner.Get() != Requester)
    {
        return false;
    }
    TentacleReservationOwner = Requester;
    return true;
}

void ACMDroppedPartActor::ReleaseTentacleReservation(AActor* Requester)
{
    if (HasAuthority() && TentacleReservationOwner.Get() == Requester)
    {
        TentacleReservationOwner.Reset();
    }
}

bool ACMDroppedPartActor::IsReservedForTentacle(
    const AActor* Requester) const
{
    return TentacleReservationOwner.IsValid()
        && TentacleReservationOwner.Get() != Requester;
}

bool ACMDroppedPartActor::IsReservedByTentacle(
    const AActor* Requester) const
{
    return IsValid(Requester)
        && TentacleReservationOwner.Get() == Requester;
}

bool ACMDroppedPartActor::BeginTentaclePull(AActor* Requester)
{
    if (!HasAuthority()
        || TentacleReservationOwner.Get() != Requester
        || bConsumed
        || !PartMesh)
    {
        return false;
    }

    bTentaclePulled = true;
    PartMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
    PartMesh->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
    ApplyVisualDefinition();
    ForceNetUpdate();
    return true;
}

void ACMDroppedPartActor::EndTentaclePull(AActor* Requester)
{
    if (!HasAuthority() || TentacleReservationOwner.Get() != Requester)
    {
        return;
    }
    bTentaclePulled = false;
    ApplyVisualDefinition();
    ForceNetUpdate();
}

bool ACMDroppedPartActor::ConsumeIntoPartSlot(
    ACMChimera* Chimera,
    const FCMPartSlotAddress& PartSlotAddress)
{
    UCMPartSlotComponent* PartSlot = Chimera
        ? Chimera->GetPartSlotComponent(PartSlotAddress)
        : nullptr;
    if (!HasAuthority()
        || bConsumed
        || !bTentaclePulled
        || !TentacleReservationOwner.IsValid()
        || !UsablePartClass
        || !PartSlot
        || PartSlot->HasAttachedPart())
    {
        return false;
    }

    ACMPartActorBase* UsablePart = ACMPartActorBase::SpawnPartFromDataRows(
        this,
        UsablePartClass,
        NAME_None,
        NAME_None,
        PartSlot->GetComponentTransform(),
        Chimera);
    if (!UsablePart)
    {
        return false;
    }

    if (!Chimera->AttachPartToSlot(PartSlotAddress, UsablePart))
    {
        UsablePart->Destroy();
        return false;
    }

    bConsumed = true;
    TentacleReservationOwner.Reset();
    Destroy();
    return true;
}

ACMPartActorBase* ACMDroppedPartActor::CreateUsablePart(
    const FTransform& SpawnTransform,
    AActor* NewOwner
)
{
    if (!HasAuthority()
        || bConsumed
        || bTentaclePulled
        || TentacleReservationOwner.IsValid()
        || !UsablePartClass)
    {
        return nullptr;
    }

    ACMPartActorBase* UsablePart = ACMPartActorBase::SpawnPartFromDataRows(
        this,
        UsablePartClass,
        NAME_None,
        NAME_None,
        SpawnTransform,
        NewOwner
    );
    if (UsablePart)
    {
        bConsumed = true;
        Destroy();
    }
    return UsablePart;
}
