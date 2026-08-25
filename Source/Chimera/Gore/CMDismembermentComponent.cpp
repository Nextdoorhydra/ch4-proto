#include "Gore/CMDismembermentComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Messaging/CMGoreMessages.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Materials/MaterialInterface.h"
#include "Data/CMBloodDefinition.h"
#include "Components/CMBloodPoolSourceComponent.h"
#include "Runtime/CMBloodSubsystem.h"
#include "Runtime/Surface/CMBloodSurfaceSubsystem.h"
#include "Tags/CMGoreGameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogCMDismemberment, Log, All);

UCMDismembermentComponent::UCMDismembermentComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
    FallbackParts = UCMDismembermentDefinition::MakeDefaultHumanParts();
    FleshChunkMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
        TEXT("/Engine/BasicShapes/Cube.Cube")));
    FleshChunkMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
        TEXT("/Game/ZombiSkinMaterial/Material/MI_ZombieSkin_Inst.MI_ZombieSkin_Inst")));
}

void UCMDismembermentComponent::BeginPlay()
{
    Super::BeginPlay();

    InitializePartStates();
    if (bConfigureLeaderPoseOnBeginPlay)
    {
        ConfigureLeaderPose();
    }
}

void UCMDismembermentComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UCMDismembermentComponent, bCorpseRagdoll);
}

bool UCMDismembermentComponent::ConfigureLeaderPose()
{
    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    USkeletalMeshComponent* LeaderMesh = Character
        ? Character->GetMesh()
        : nullptr;
    if (!LeaderMesh)
    {
        UE_LOG(LogCMDismemberment, Warning,
            TEXT("[Dismemberment] Owner '%s' is not a Character with a mesh."),
            *GetNameSafe(GetOwner()));
        return false;
    }

    int32 ConfiguredCount = 0;
    for (const FCMDismembermentPartDefinition& Part
        : GetEffectivePartDefinitions())
    {
        USkeletalMeshComponent* PartMesh = FindPartMesh(Part.ComponentName);
        if (!PartMesh)
        {
            UE_LOG(LogCMDismemberment, Warning,
                TEXT("[Dismemberment] Missing part component '%s' on '%s'."),
                *Part.ComponentName.ToString(),
                *GetNameSafe(GetOwner()));
            continue;
        }

        PartMesh->SetLeaderPoseComponent(LeaderMesh, true, false);
        ++ConfiguredCount;
    }

    UE_LOG(LogCMDismemberment, Log,
        TEXT("[Dismemberment] Configured %d/%d leader-pose parts on '%s'."),
        ConfiguredCount,
        GetEffectivePartDefinitions().Num(),
        *GetNameSafe(GetOwner()));

    return ConfiguredCount == GetEffectivePartDefinitions().Num();
}

bool UCMDismembermentComponent::EnterCorpseRagdoll()
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() || bCorpseRagdoll)
    {
        return false;
    }

    ACharacter* Character = Cast<ACharacter>(Owner);
    USkeletalMeshComponent* LeaderMesh = Character
        ? Character->GetMesh()
        : nullptr;
    if (!Character || !LeaderMesh || !LeaderMesh->GetPhysicsAsset())
    {
        UE_LOG(LogCMDismemberment, Error,
            TEXT("[Dismemberment] '%s' cannot ragdoll: Character mesh or PhysicsAsset is missing."),
            *GetNameSafe(Owner));
        return false;
    }

    bCorpseRagdoll = true;
    ApplyCorpseRagdoll();
    Owner->ForceNetUpdate();
    return true;
}

bool UCMDismembermentComponent::SeverBodyPart(
    const ECMBodyPart BodyPart,
    const FVector HitLocation,
    const FVector Impulse
)
{
    AActor* Owner = GetOwner();
    ECMBodyPartState* State = PartStates.Find(BodyPart);
    const bool bCanSever = State &&
        (*State == ECMBodyPartState::AttachedAlive ||
         *State == ECMBodyPartState::CorpseAttached);
    if (!Owner || !Owner->HasAuthority() || !bCanSever)
    {
        return false;
    }

    const FCMDismembermentPartDefinition* Part =
        GetEffectivePartDefinitions().FindByPredicate(
            [BodyPart](const FCMDismembermentPartDefinition& Entry)
            {
                return Entry.BodyPart == BodyPart;
            });
    USkeletalMeshComponent* AttachedMesh = Part
        ? FindPartMesh(Part->ComponentName)
        : nullptr;
    USkeletalMesh* DetachedMesh = Part
        ? Part->DetachedMesh.LoadSynchronous()
        : nullptr;
    UPhysicsAsset* DetachedPhysicsAsset = Part
        ? Part->DetachedPhysicsAsset.LoadSynchronous()
        : nullptr;
    UWorld* World = GetWorld();
    if (!Part || !AttachedMesh || !DetachedMesh ||
        !DetachedPhysicsAsset || !World)
    {
        UE_LOG(LogCMDismemberment, Error,
            TEXT("[Dismemberment] Cannot sever part %d on '%s': required mesh data is missing."),
            static_cast<int32>(BodyPart),
            *GetNameSafe(Owner));
        return false;
    }

    ASkeletalMeshActor* DetachedActor =
        World->SpawnActorDeferred<ASkeletalMeshActor>(
            ASkeletalMeshActor::StaticClass(),
            AttachedMesh->GetComponentTransform(),
            Owner,
            nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!DetachedActor)
    {
        return false;
    }

    USkeletalMeshComponent* DetachedComponent =
        DetachedActor->GetSkeletalMeshComponent();
    DetachedComponent->SetSkeletalMeshAsset(DetachedMesh);
    DetachedComponent->SetPhysicsAsset(DetachedPhysicsAsset, false);
    DetachedComponent->SetCollisionProfileName(RagdollCollisionProfileName);
    DetachedActor->Tags.AddUnique(TEXT("CM.DetachedBodyPart"));
    DetachedActor->FinishSpawning(AttachedMesh->GetComponentTransform());
    DetachedActor->SetLifeSpan(SpawnedGoreLifeSpan);

    DetachedComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    DetachedComponent->SetAllBodiesNotifyRigidBodyCollision(true);
    DetachedComponent->OnComponentHit.AddUniqueDynamic(
        this,
        &UCMDismembermentComponent::HandleDetachedPartHit);
    DetachedComponent->SetAllBodiesSimulatePhysics(true);
    DetachedComponent->SetSimulatePhysics(true);
    DetachedComponent->WakeAllRigidBodies();

    FVector AppliedImpulse = Impulse * Part->ImpulseMultiplier;
    AppliedImpulse = AppliedImpulse.GetClampedToMaxSize(Part->MaxImpulse);
    if (!AppliedImpulse.IsNearlyZero())
    {
        DetachedComponent->AddImpulse(AppliedImpulse, NAME_None, false);
    }

    AttachedMesh->SetVisibility(false, true);
    AttachedMesh->SetHiddenInGame(true, true);
    *State = ECMBodyPartState::Severed;
    DetachedPartActors.Add(BodyPart, DetachedActor);

    const FVector BurstDirection = AppliedImpulse.GetSafeNormal(
        SMALL_NUMBER,
        FVector::UpVector);
    const FVector BurstLocation = HitLocation.IsNearlyZero()
        ? AttachedMesh->GetComponentLocation()
        : HitLocation;
    SpawnFleshChunks(BurstLocation, BurstDirection);
    BroadcastBloodBurst(BurstLocation, BurstDirection);
    TrySpawnGroundBloodDecal(BurstLocation, DetachedActor);
    Owner->ForceNetUpdate();

    UE_LOG(LogCMDismemberment, Log,
        TEXT("[Dismemberment] Severed part %d on '%s': impulse=%.1f chunks=%d."),
        static_cast<int32>(BodyPart),
        *GetNameSafe(Owner),
        AppliedImpulse.Size(),
        FleshChunkCount);
    return true;
}

ECMBodyPartState UCMDismembermentComponent::GetPartState(
    const ECMBodyPart BodyPart
) const
{
    const ECMBodyPartState* State = PartStates.Find(BodyPart);
    return State ? *State : ECMBodyPartState::Destroyed;
}

ECMBodyPart UCMDismembermentComponent::ResolveBodyPartFromComponent(
    const UActorComponent* Component
) const
{
    if (!Component)
    {
        return ECMBodyPart::None;
    }

    const FCMDismembermentPartDefinition* DefinitionEntry =
        GetEffectivePartDefinitions().FindByPredicate(
            [Component](const FCMDismembermentPartDefinition& Part)
            {
                return Part.ComponentName == Component->GetFName();
            });
    return DefinitionEntry
        ? DefinitionEntry->BodyPart
        : ECMBodyPart::None;
}

int32 UCMDismembermentComponent::GetConfiguredPartCount() const
{
    return GetEffectivePartDefinitions().Num();
}

AActor* UCMDismembermentComponent::GetDetachedPartActor(
    const ECMBodyPart BodyPart
) const
{
    const TObjectPtr<AActor>* Actor = DetachedPartActors.Find(BodyPart);
    return Actor ? Actor->Get() : nullptr;
}

int32 UCMDismembermentComponent::GetSpawnedFleshChunkCount() const
{
    int32 Count = 0;
    for (const AActor* Chunk : SpawnedFleshChunks)
    {
        Count += IsValid(Chunk) ? 1 : 0;
    }
    return Count;
}

bool UCMDismembermentComponent::IsCorpseBloodPoolActive() const
{
    return CorpseBloodPoolComponent &&
        CorpseBloodPoolComponent->IsBloodPoolActive();
}

void UCMDismembermentComponent::OnRep_CorpseRagdoll()
{
    if (bCorpseRagdoll)
    {
        ApplyCorpseRagdoll();
    }
}

void UCMDismembermentComponent::InitializePartStates()
{
    PartStates.Reset();
    for (const FCMDismembermentPartDefinition& Part
        : GetEffectivePartDefinitions())
    {
        if (Part.BodyPart != ECMBodyPart::None)
        {
            PartStates.Add(Part.BodyPart, ECMBodyPartState::AttachedAlive);
        }
    }
}

void UCMDismembermentComponent::ApplyCorpseRagdoll()
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character)
    {
        return;
    }

    for (TPair<ECMBodyPart, ECMBodyPartState>& Pair : PartStates)
    {
        if (Pair.Value == ECMBodyPartState::AttachedAlive)
        {
            Pair.Value = ECMBodyPartState::CorpseAttached;
        }
    }

    ConfigureLeaderPose();

    if (UCharacterMovementComponent* Movement =
        Character->GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
        Movement->DisableMovement();
    }

    if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    USkeletalMeshComponent* LeaderMesh = Character->GetMesh();
    if (!LeaderMesh)
    {
        return;
    }

    LeaderMesh->SetCollisionProfileName(RagdollCollisionProfileName);
    LeaderMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    LeaderMesh->SetAllBodiesSimulatePhysics(true);
    LeaderMesh->SetSimulatePhysics(true);
    LeaderMesh->WakeAllRigidBodies();
    LeaderMesh->bBlendPhysics = true;

    StartCorpseBloodPool();

    OnCorpseRagdollStarted.Broadcast();

    UE_LOG(LogCMDismemberment, Log,
        TEXT("[Dismemberment] Corpse ragdoll started for '%s' with %d attached parts."),
        *GetNameSafe(Character),
        PartStates.Num());
}

bool UCMDismembermentComponent::StartCorpseBloodPool()
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!bSpawnCorpseBloodPool || !Character)
    {
        return false;
    }

    if (IsCorpseBloodPoolActive())
    {
        return true;
    }

    if (!CorpseBloodPoolComponent)
    {
        CorpseBloodPoolComponent =
            NewObject<UCMBloodPoolSourceComponent>(
                Character,
                TEXT("CorpseBloodPool"));
        if (!CorpseBloodPoolComponent)
        {
            return false;
        }

        CorpseBloodPoolComponent->BloodDefinitionId = BloodDefinitionId;
        CorpseBloodPoolComponent->GrowthDurationSeconds =
            CorpseBloodPoolGrowthDuration;
        CorpseBloodPoolComponent->LifetimeSeconds =
            CorpseBloodPoolLifetime;
        CorpseBloodPoolComponent->TraceDistance =
            GroundBloodTraceDistance;
        Character->AddInstanceComponent(CorpseBloodPoolComponent);
        CorpseBloodPoolComponent->RegisterComponent();
    }

    FVector TorsoLocation = Character->GetActorLocation();
    if (AActor* DetachedTorso =
        GetDetachedPartActor(ECMBodyPart::Torso))
    {
        TorsoLocation = DetachedTorso->GetComponentsBoundingBox().GetCenter();
    }
    else if (USkeletalMeshComponent* TorsoMesh = FindPartMesh(TEXT("Torso")))
    {
        TorsoLocation = TorsoMesh->Bounds.Origin;
    }

    const bool bStarted = CorpseBloodPoolComponent->StartBloodPool(
        TorsoLocation,
        FVector::UpVector,
        CorpseBloodPoolAmount,
        BloodDefinitionId);
    if (bStarted)
    {
        UE_LOG(LogCMDismemberment, Log,
            TEXT("[Dismemberment] Corpse torso blood pool for '%s': STARTED at %s."),
            *GetNameSafe(Character),
            *TorsoLocation.ToCompactString());
    }
    else
    {
        UE_LOG(LogCMDismemberment, Warning,
            TEXT("[Dismemberment] Corpse torso blood pool for '%s': FAILED at %s."),
            *GetNameSafe(Character),
            *TorsoLocation.ToCompactString());
    }
    return bStarted;
}

const TArray<FCMDismembermentPartDefinition>&
UCMDismembermentComponent::GetEffectivePartDefinitions() const
{
    return Definition ? Definition->Parts : FallbackParts;
}

USkeletalMeshComponent* UCMDismembermentComponent::FindPartMesh(
    const FName ComponentName
) const
{
    const AActor* Owner = GetOwner();
    if (!Owner || ComponentName.IsNone())
    {
        return nullptr;
    }

    TInlineComponentArray<USkeletalMeshComponent*> MeshComponents;
    Owner->GetComponents(MeshComponents);
    for (USkeletalMeshComponent* MeshComponent : MeshComponents)
    {
        if (MeshComponent && MeshComponent->GetFName() == ComponentName)
        {
            return MeshComponent;
        }
    }
    return nullptr;
}

void UCMDismembermentComponent::SpawnFleshChunks(
    const FVector& Location,
    const FVector& Direction
)
{
    UWorld* World = GetWorld();
    UStaticMesh* Mesh = FleshChunkMesh.LoadSynchronous();
    UMaterialInterface* Material = FleshChunkMaterial.LoadSynchronous();
    if (!World || !Mesh || FleshChunkCount <= 0)
    {
        return;
    }

    for (int32 Index = 0; Index < FleshChunkCount; ++Index)
    {
        const float Scale = FMath::FRandRange(0.06f, 0.14f);
        const FVector SpawnLocation = Location + FMath::VRand() * 8.0f;
        const FTransform Transform(
            FMath::VRand().Rotation(),
            SpawnLocation,
            FVector(Scale));
        AStaticMeshActor* Chunk = World->SpawnActorDeferred<AStaticMeshActor>(
            AStaticMeshActor::StaticClass(),
            Transform,
            GetOwner(),
            nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Chunk)
        {
            continue;
        }

        UStaticMeshComponent* ChunkMesh = Chunk->GetStaticMeshComponent();
        ChunkMesh->SetMobility(EComponentMobility::Movable);
        ChunkMesh->SetStaticMesh(Mesh);
        if (Material)
        {
            ChunkMesh->SetMaterial(0, Material);
        }
        ChunkMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
        Chunk->Tags.AddUnique(TEXT("CM.FleshChunk"));
        Chunk->FinishSpawning(Transform);
        Chunk->SetLifeSpan(SpawnedGoreLifeSpan);

        ChunkMesh->SetSimulatePhysics(true);
        const FVector ScatterDirection =
            (Direction + FMath::VRand() * 0.75f).GetSafeNormal(
                SMALL_NUMBER,
                FVector::UpVector);
        ChunkMesh->AddImpulse(
            ScatterDirection * FMath::FRandRange(
                FleshChunkImpulse * 0.6f,
                FleshChunkImpulse));
        SpawnedFleshChunks.Add(Chunk);
    }
}

void UCMDismembermentComponent::BroadcastBloodBurst(
    const FVector& Location,
    const FVector& Direction
)
{
    FCMBloodBurstMessage Message;
    Message.Source = GetOwner();
    Message.Location = Location;
    Message.Direction = Direction;
    Message.Amount = BloodBurstAmount;
    Message.BloodDefinitionId = BloodDefinitionId;

    UGameplayMessageSubsystem::Get(this).BroadcastMessage(
        CMGoreGameplayTags::Message::Blood::Burst,
        Message);
    ++BroadcastBloodBurstCount;
}

bool UCMDismembermentComponent::TrySpawnGroundBloodDecal(
    const FVector& Location,
    AActor* IgnoredActor
)
{
    UWorld* World = GetWorld();
    if (!bSpawnBloodDecals || !World || GroundBloodTraceDistance <= 0.0f)
    {
        return false;
    }

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(CMDismembermentGroundBlood),
        false,
        GetOwner());
    if (IgnoredActor)
    {
        QueryParams.AddIgnoredActor(IgnoredActor);
    }

    FHitResult Hit;
    const FVector TraceStart = Location + FVector::UpVector * 5.0f;
    const FVector TraceEnd = Location -
        FVector::UpVector * GroundBloodTraceDistance;
    if (!World->LineTraceSingleByChannel(
        Hit,
        TraceStart,
        TraceEnd,
        ECC_Visibility,
        QueryParams))
    {
        return false;
    }

    return SpawnBloodDecalAtSurface(
        Hit.ImpactPoint,
        Hit.ImpactNormal,
        IgnoredActor);
}

bool UCMDismembermentComponent::SpawnBloodDecalAtSurface(
    const FVector& SurfaceLocation,
    const FVector& SurfaceNormal,
    AActor* IgnoredActor
)
{
    UWorld* World = GetWorld();
    UCMBloodSubsystem* BloodSubsystem = World
        ? World->GetSubsystem<UCMBloodSubsystem>()
        : nullptr;
    UCMBloodSurfaceSubsystem* SurfaceSubsystem = World
        ? World->GetSubsystem<UCMBloodSurfaceSubsystem>()
        : nullptr;
    const UCMBloodDefinition* BloodDefinition = BloodSubsystem
        ? BloodSubsystem->ResolveBloodDefinition(BloodDefinitionId)
        : nullptr;
    if (!bSpawnBloodDecals || !BloodDefinition ||
        !BloodDefinition->Surface.bEnabled ||
        !IsValid(BloodDefinition->Surface.DecalMaterial) ||
        !SurfaceSubsystem)
    {
        return false;
    }

    const FVector Normal = SurfaceNormal.GetSafeNormal(
        SMALL_NUMBER,
        FVector::UpVector);
    FCMBloodSurfaceBurstRequest Request;
    Request.Origin = SurfaceLocation + Normal * 5.0f;
    Request.Direction = -Normal;
    Request.SampleCount = 1;
    Request.TraceDistance = 12.0f;
    Request.ConeHalfAngleDegrees = 0.0f;
    Request.DecalMaterial = BloodDefinition->Surface.DecalMaterial;
    Request.DecalActorClass = BloodDefinition->Surface.DecalActorClass;
    Request.DecalExtentRange = BloodDefinition->Surface.DecalExtentRange;
    Request.DecalDepth = BloodDefinition->Surface.DecalDepth;
    Request.LifetimeSeconds = BloodDefinition->Surface.LifetimeSeconds;
    Request.FadeDurationSeconds =
        BloodDefinition->Surface.FadeDurationSeconds;
    Request.PresentationMagnitude = BloodBurstAmount;
    Request.IgnoredActor = IgnoredActor;

    const bool bSpawned =
        !SurfaceSubsystem->SpawnSurfaceBurst(Request).IsEmpty();
    SpawnedBloodDecalCount += bSpawned ? 1 : 0;
    return bSpawned;
}

void UCMDismembermentComponent::HandleDetachedPartHit(
    UPrimitiveComponent* HitComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    const FVector NormalImpulse,
    const FHitResult& Hit
)
{
    if (!bSpawnBloodDecals || !HitComponent ||
        MaxBloodTrailDecalsPerPart <= 0 ||
        (OtherActor && OtherActor->ActorHasTag(TEXT("CM.DetachedBodyPart"))))
    {
        return;
    }

    const TWeakObjectPtr<UPrimitiveComponent> ComponentKey(HitComponent);
    const int32 ExistingDecalCount =
        BloodTrailDecalCounts.FindRef(ComponentKey);
    if (ExistingDecalCount >= MaxBloodTrailDecalsPerPart ||
        HitComponent->GetPhysicsLinearVelocity().Size() < BloodTrailMinSpeed)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const FVector ContactLocation = Hit.ImpactPoint.IsNearlyZero()
        ? HitComponent->Bounds.Origin
        : FVector(Hit.ImpactPoint);
    const double CurrentTime = World->GetTimeSeconds();
    if (const FVector* LastLocation =
        LastBloodTrailLocations.Find(ComponentKey))
    {
        const double LastTime = LastBloodTrailTimes.FindRef(ComponentKey);
        if (FVector::DistSquared(*LastLocation, ContactLocation) <
                FMath::Square(BloodTrailMinDistance) ||
            CurrentTime - LastTime < BloodTrailMinIntervalSeconds)
        {
            return;
        }
    }

    if (SpawnBloodDecalAtSurface(
        ContactLocation,
        Hit.ImpactNormal,
        HitComponent->GetOwner()))
    {
        LastBloodTrailLocations.Add(ComponentKey, ContactLocation);
        LastBloodTrailTimes.Add(ComponentKey, CurrentTime);
        BloodTrailDecalCounts.Add(ComponentKey, ExistingDecalCount + 1);
    }
}
