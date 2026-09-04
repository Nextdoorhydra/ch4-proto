#include "Gore/CMGoreResponseComponent.h"

#include "Components/CMBloodPoolSourceComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/CMBloodDefinition.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Messaging/CMGoreMessages.h"
#include "Runtime/CMBloodSubsystem.h"
#include "Runtime/Surface/CMBloodSurfaceSubsystem.h"
#include "Tags/CMGoreGameplayTags.h"

UCMGoreResponseComponent::UCMGoreResponseComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    FleshChunkMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
        TEXT("/Engine/BasicShapes/Cube.Cube")));
    FleshChunkMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
        TEXT("/Game/ZombiSkinMaterial/Material/MI_ZombieSkin_Inst.MI_ZombieSkin_Inst")));
}

void UCMGoreResponseComponent::SpawnHitEffects(
    const FVector HitLocation,
    const FVector SurfaceNormal,
    const FVector BloodDirection,
    const float Intensity)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        return;
    }

    BroadcastBloodImpact(
        HitLocation,
        SurfaceNormal,
        BloodDirection,
        FMath::Max(0.0f, Intensity));
    SpawnGroundBloodDecal(HitLocation);
}

void UCMGoreResponseComponent::SpawnDestructionEffects(
    const FVector Location,
    const FVector Direction)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        return;
    }

    const FVector SafeDirection = Direction.GetSafeNormal(
        SMALL_NUMBER,
        FVector::UpVector);
    SpawnFleshChunks(Location, SafeDirection);
    BroadcastBloodBurst(Location, SafeDirection);
    StartBloodPool(Location);
}

void UCMGoreResponseComponent::BroadcastBloodImpact(
    const FVector& Location,
    const FVector& SurfaceNormal,
    const FVector& Direction,
    const float Intensity)
{
    FCMBloodImpactMessage Message;
    Message.Source = GetOwner();
    Message.Location = Location;
    Message.SurfaceNormal = SurfaceNormal.GetSafeNormal(
        SMALL_NUMBER,
        FVector::UpVector);
    Message.Direction = Direction.GetSafeNormal(
        SMALL_NUMBER,
        Message.SurfaceNormal);
    Message.Intensity = Intensity;
    Message.BloodDefinitionId = BloodDefinitionId;

    UGameplayMessageSubsystem::Get(this).BroadcastMessage(
        CMGoreGameplayTags::Message::Blood::Impact,
        Message);
}

void UCMGoreResponseComponent::BroadcastBloodBurst(
    const FVector& Location,
    const FVector& Direction)
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
}

void UCMGoreResponseComponent::SpawnFleshChunks(
    const FVector& Location,
    const FVector& Direction)
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

bool UCMGoreResponseComponent::SpawnGroundBloodDecal(
    const FVector& Location)
{
    UWorld* World = GetWorld();
    AActor* Owner = GetOwner();
    if (!bSpawnGroundBloodDecal || !World || !Owner ||
        GroundBloodTraceDistance <= 0.0f)
    {
        return false;
    }

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(CMChimeraGroundBlood),
        false,
        Owner);
    if (AActor* OwningActor = Owner->GetOwner())
    {
        QueryParams.AddIgnoredActor(OwningActor);
    }
    FHitResult Hit;
    if (!World->LineTraceSingleByChannel(
        Hit,
        Location + FVector::UpVector * 5.0f,
        Location - FVector::UpVector * GroundBloodTraceDistance,
        ECC_Visibility,
        QueryParams))
    {
        return false;
    }

    UCMBloodSubsystem* BloodSubsystem =
        World->GetSubsystem<UCMBloodSubsystem>();
    UCMBloodSurfaceSubsystem* SurfaceSubsystem =
        World->GetSubsystem<UCMBloodSurfaceSubsystem>();
    const UCMBloodDefinition* Definition = BloodSubsystem
        ? BloodSubsystem->ResolveBloodDefinition(BloodDefinitionId)
        : nullptr;
    if (!Definition || !Definition->Surface.bEnabled ||
        !IsValid(Definition->Surface.DecalMaterial) || !SurfaceSubsystem)
    {
        return false;
    }

    const FVector Normal = Hit.ImpactNormal.GetSafeNormal(
        SMALL_NUMBER,
        FVector::UpVector);
    FCMBloodSurfaceBurstRequest Request;
    Request.Origin = Hit.ImpactPoint + Normal * 5.0f;
    Request.ResidueType = ECMBloodResidueType::Splash;
    Request.BloodDefinitionId = BloodDefinitionId;
    Request.SourceActor = Owner;
    Request.Direction = -Normal;
    Request.SampleCount = 1;
    Request.TraceDistance = 12.0f;
    Request.ConeHalfAngleDegrees = 0.0f;
    Request.DecalMaterial = Definition->Surface.DecalMaterial;
    Request.DecalActorClass = Definition->Surface.DecalActorClass;
    Request.DecalExtentRange = Definition->Surface.DecalExtentRange;
    Request.DecalDepth = Definition->Surface.DecalDepth;
    Request.LifetimeSeconds = Definition->Surface.LifetimeSeconds;
    Request.FadeDurationSeconds = Definition->Surface.FadeDurationSeconds;
    Request.PresentationMagnitude = 1.0f;
    Request.IgnoredActor = Owner;
    return !SurfaceSubsystem->SpawnSurfaceBurst(Request).IsEmpty();
}

bool UCMGoreResponseComponent::StartBloodPool(const FVector& Location)
{
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!bSpawnBloodPoolOnDestruction || !Owner || !World ||
        BloodPoolAmount <= 0.0f || GroundBloodTraceDistance <= 0.0f)
    {
        return false;
    }

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(CMChimeraBloodPoolGround),
        false,
        Owner);
    if (AActor* OwningActor = Owner->GetOwner())
    {
        QueryParams.AddIgnoredActor(OwningActor);
    }
    FHitResult GroundHit;
    if (!World->LineTraceSingleByChannel(
        GroundHit,
        Location + FVector::UpVector * 5.0f,
        Location - FVector::UpVector * GroundBloodTraceDistance,
        ECC_Visibility,
        QueryParams))
    {
        return false;
    }

    const FName ComponentName = MakeUniqueObjectName(
        Owner,
        UCMBloodPoolSourceComponent::StaticClass(),
        TEXT("DestructionBloodPool"));
    UCMBloodPoolSourceComponent* Pool =
        NewObject<UCMBloodPoolSourceComponent>(Owner, ComponentName);
    if (!Pool)
    {
        return false;
    }

    Pool->BloodDefinitionId = BloodDefinitionId;
    Pool->GrowthDurationSeconds = BloodPoolGrowthDuration;
    Pool->LifetimeSeconds = BloodPoolLifetime;
    Pool->TraceDistance = GroundBloodTraceDistance;
    Owner->AddInstanceComponent(Pool);
    Pool->RegisterComponent();
    const bool bStarted = Pool->StartBloodPool(
        GroundHit.ImpactPoint,
        GroundHit.ImpactNormal,
        BloodPoolAmount,
        BloodDefinitionId);
    if (!bStarted)
    {
        Pool->DestroyComponent();
        return false;
    }
    BloodPoolComponents.Add(Pool);
    return true;
}
