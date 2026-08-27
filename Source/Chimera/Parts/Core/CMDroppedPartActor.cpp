#include "Parts/Core/CMDroppedPartActor.h"

#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Parts/Core/CMPartActorBase.h"

ACMDroppedPartActor::ACMDroppedPartActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    PartMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PartMesh"));
    SetRootComponent(PartMesh);
    PartMesh->SetIsReplicated(true);
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
    PartMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    PartMesh->SetAllBodiesSimulatePhysics(true);
    PartMesh->SetSimulatePhysics(true);
    PartMesh->WakeAllRigidBodies();
}

ACMPartActorBase* ACMDroppedPartActor::CreateUsablePart(
    const FTransform& SpawnTransform,
    AActor* NewOwner
)
{
    if (!HasAuthority() || bConsumed || !UsablePartClass)
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
