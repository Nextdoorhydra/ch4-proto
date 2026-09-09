#include "Parts/Core/CMDroppedPartActor.h"

#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Parts/Tentacle/CMTentacleSegmentActor.h"
#include "Player/CMChimera.h"
#include "Player/CMPartSlotComponent.h"

ACMDroppedPartActor::ACMDroppedPartActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    bReplicates = true;
    SetReplicateMovement(true);
    SetNetUpdateFrequency(30.0f);
    SetMinNetUpdateFrequency(10.0f);

    PartMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PartMesh"));
    SetRootComponent(PartMesh);
    PartMesh->SetIsReplicated(true);
    PartMesh->SetGenerateOverlapEvents(true);
    Tags.AddUnique(
        ACMTentacleSegmentActor::TentacleInteractiveActorTag);
}

void ACMDroppedPartActor::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (HasAuthority()
        && !bConsumed
        && !bTentaclePulled
        && PartMesh
        && PartMesh->IsSimulatingPhysics())
    {
        UpdateAuthoritativePickupLocation();
    }
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
    DOREPLIFETIME(ACMDroppedPartActor, AuthoritativePickupLocation);
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
    UpdateAuthoritativePickupLocation();
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
    const bool bServerSimulatesLoosePart = HasAuthority()
        && !bTentaclePulled;
    if (bServerSimulatesLoosePart)
    {
        PartMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        PartMesh->SetGenerateOverlapEvents(true);
        PartMesh->SetAllBodiesSimulatePhysics(true);
        PartMesh->SetSimulatePhysics(true);
        SetReplicateMovement(false);
        PartMesh->WakeAllRigidBodies();
        UpdateAuthoritativePickupLocation();
        SetActorTickEnabled(true);
    }
    else
    {
        PartMesh->SetAllBodiesSimulatePhysics(false);
        PartMesh->SetSimulatePhysics(false);
        PartMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        PartMesh->SetGenerateOverlapEvents(false);
        SetActorTickEnabled(false);
        if (!HasAuthority())
        {
            ApplyAuthoritativePickupLocation();
        }
    }
}

void ACMDroppedPartActor::OnRep_TentaclePulled()
{
    ApplyVisualDefinition();
}

void ACMDroppedPartActor::OnRep_AuthoritativePickupLocation()
{
    ApplyAuthoritativePickupLocation();
}

FVector ACMDroppedPartActor::GetAuthoritativePickupLocation() const
{
    return bTentaclePulled
        ? GetActorLocation()
        : FVector(AuthoritativePickupLocation);
}

void ACMDroppedPartActor::UpdateAuthoritativePickupLocation()
{
    if (!HasAuthority() || !PartMesh)
    {
        return;
    }

    PartMesh->UpdateBounds();
    AuthoritativePickupLocation = PartMesh->Bounds.Origin;
}

void ACMDroppedPartActor::ApplyAuthoritativePickupLocation()
{
    if (HasAuthority() || bTentaclePulled || !PartMesh)
    {
        return;
    }

    PartMesh->UpdateBounds();
    PartMesh->AddWorldOffset(
        FVector(AuthoritativePickupLocation) - PartMesh->Bounds.Origin,
        false,
        nullptr,
        ETeleportType::TeleportPhysics);
    PartMesh->UpdateBounds();
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

    PartMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
    PartMesh->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
    UpdateAuthoritativePickupLocation();
    bTentaclePulled = true;
    ApplyVisualDefinition();
    SetReplicateMovement(true);
    SetActorLocation(
        AuthoritativePickupLocation,
        false,
        nullptr,
        ETeleportType::TeleportPhysics);
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
    AuthoritativePickupLocation = GetActorLocation();
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
