#include "Stage/Obstacle/CMPushBox.h"

#include "Components/AudioComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Parts/Arm/CMArmPart.h"
#include "Player/CMChimera.h"
#include "Sound/CMGameSoundBridgeSubsystem.h"
#include "Sound/CMSoundPlayback.h"
#include "Sound/CMSoundTags.h"
#include "Stage/Trigger/Component/CMMechanismWeightComponent.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraPushBox, Log, All);

ACMPushBox::ACMPushBox()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    bReplicates = true;
    SetReplicateMovement(true);

    BoxMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoxMesh"));
    SetRootComponent(BoxMesh);
    BoxMesh->SetMobility(EComponentMobility::Movable);
    BoxMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    BoxMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    BoxMesh->SetGenerateOverlapEvents(true);
    BoxMesh->SetNotifyRigidBodyCollision(true);
    BoxMesh->SetSimulatePhysics(false);
    BoxMesh->SetIsReplicated(true);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMeshAsset.Succeeded())
    {
        BoxMesh->SetStaticMesh(CubeMeshAsset.Object);
    }

    MechanismWeight = CreateDefaultSubobject<UCMMechanismWeightComponent>(TEXT("MechanismWeight"));
}

void ACMPushBox::BeginPlay()
{
    Super::BeginPlay();

    InitialTransform = GetActorTransform();

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UCMGameSoundBridgeSubsystem* SoundBridge = GameInstance->GetSubsystem<UCMGameSoundBridgeSubsystem>())
        {
            SoundBridge->OnSoundCatalogsRebuilt.AddUObject(this, &ThisClass::HandleSoundCatalogsRebuilt);
        }
    }

    SetActorTickEnabled(true);
    ApplyEditorSettings();
    BoxMesh->OnComponentHit.AddUniqueDynamic(this, &ThisClass::HandleBoxHit);
    UE_LOG(LogChimeraPushBox, Log, TEXT("[PushBox Ready] Box=%s Weight=%.1f GenerateOverlap=%s Location=%s Extent=%s"), *GetName(), MechanismWeight ? MechanismWeight->GetMechanismWeight() : 0.0f, BoxMesh && BoxMesh->GetGenerateOverlapEvents() ? TEXT("true") : TEXT("false"), *GetActorLocation().ToCompactString(), BoxMesh ? *BoxMesh->Bounds.BoxExtent.ToCompactString() : TEXT("None"));
}

void ACMPushBox::ResetForCheckpoint()
{
    if (!HasAuthority())
    {
        return;
    }

    StopPush();
    LastAcceptedAttackId.Invalidate();
    SetActorTransform(
        InitialTransform,
        false,
        nullptr,
        ETeleportType::TeleportPhysics);
    ForceNetUpdate();
}

void ACMPushBox::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UCMGameSoundBridgeSubsystem* SoundBridge = GameInstance->GetSubsystem<UCMGameSoundBridgeSubsystem>())
        {
            SoundBridge->OnSoundCatalogsRebuilt.RemoveAll(this);
        }
    }
    StopMoveLoopSound();

    Super::EndPlay(EndPlayReason);
}

void ACMPushBox::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ThisClass, bIsMoving);
}

void ACMPushBox::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplyEditorSettings();
}

void ACMPushBox::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!HasAuthority() || RemainingPushDistance <= UE_SMALL_NUMBER || PushDirection.IsNearlyZero())
    {
        return;
    }

    const float RequestedDistance = FMath::Min(FMath::Max(PushSpeed, 0.0f) * DeltaTime, RemainingPushDistance);
    if (RequestedDistance <= UE_SMALL_NUMBER)
    {
        StopPush();
        return;
    }

    const FVector PreviousLocation = GetActorLocation();
    const FVector RequestedOffset = PushDirection * RequestedDistance;
    const FVector SweepExtent(FMath::Max(BoxMesh->Bounds.BoxExtent.X - 1.0f, 1.0f), FMath::Max(BoxMesh->Bounds.BoxExtent.Y - 1.0f, 1.0f), FMath::Max(BoxMesh->Bounds.BoxExtent.Z - 2.0f, 1.0f));
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CMPushBoxMove), false, this);
    QueryParams.AddIgnoredActors(BoxMesh->CopyArrayOfMoveIgnoreActors());
    TArray<FHitResult> SweepHits;
    GetWorld()->SweepMultiByProfile(SweepHits, BoxMesh->Bounds.Origin, BoxMesh->Bounds.Origin + RequestedOffset, FQuat::Identity, BoxMesh->GetCollisionProfileName(), FCollisionShape::MakeBox(SweepExtent), QueryParams);
    bool bMovementBlocked = false;
    float MovementFraction = 1.0f;
    for (const FHitResult& SweepHit : SweepHits)
    {
        if (!SweepHit.bBlockingHit)
        {
            continue;
        }

        const FVector BlockingNormal = SweepHit.bStartPenetrating || SweepHit.ImpactNormal.IsNearlyZero() ? SweepHit.Normal.GetSafeNormal() : SweepHit.ImpactNormal.GetSafeNormal();
        if (FVector::DotProduct(PushDirection, BlockingNormal) >= -KINDA_SMALL_NUMBER)
        {
            continue;
        }

        bMovementBlocked = true;
        MovementFraction = FMath::Min(MovementFraction, FMath::Clamp(SweepHit.Time, 0.0f, 1.0f));
    }
    SetActorLocation(PreviousLocation + RequestedOffset * MovementFraction, false);
    const float MovedDistance = FVector::Dist2D(PreviousLocation, GetActorLocation());
    RemainingPushDistance = FMath::Max(RemainingPushDistance - MovedDistance, 0.0f);

    if (bMovementBlocked || RemainingPushDistance <= UE_SMALL_NUMBER)
    {
        StopPush();
    }
}

bool ACMPushBox::ReceiveCombatHit_Implementation(const FCMCombatHitRequest& Request)
{
    if (!HasAuthority() || !IsValid(Cast<ACMArmPart>(Request.SourcePart)) || !Request.AttackId.IsValid())
    {
        return false;
    }

    if (Request.AttackId == LastAcceptedAttackId)
    {
        return true;
    }

    AActor* PushSource = IsValid(Request.Attacker) ? Request.Attacker.Get() : Request.SourcePart.Get();
    BoxMesh->IgnoreActorWhenMoving(PushSource, true);
    if (!StartPush(ResolveCardinalPushDirection(Request.ImpactPoint, Request.ImpactDirection)))
    {
        BoxMesh->IgnoreActorWhenMoving(PushSource, false);
        return false;
    }

    LastAcceptedAttackId = Request.AttackId;
    return true;
}

void ACMPushBox::HandleBoxHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
    const ACMChimera* Chimera = Cast<ACMChimera>(OtherActor);
    if (!HasAuthority() || !IsValid(Chimera) || !IsValid(OtherComponent))
    {
        return;
    }

    FVector PlayerVelocity = OtherComponent->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint);
    PlayerVelocity.Z = 0.0f;
    FVector ContactDirection = GetActorLocation() - OtherComponent->Bounds.Origin;
    ContactDirection.Z = 0.0f;
    const bool bHasPlanarCollisionImpulse = FVector(NormalImpulse.X, NormalImpulse.Y, 0.0f).SizeSquared() > UE_SMALL_NUMBER;
    const bool bMovingTowardBox = FVector::DotProduct(PlayerVelocity, ContactDirection.GetSafeNormal()) >= FMath::Max(MinimumPlayerImpactSpeed, 0.0f);
    if (!ContactDirection.Normalize() || (!bHasPlanarCollisionImpulse && !bMovingTowardBox))
    {
        return;
    }

    BoxMesh->IgnoreActorWhenMoving(OtherActor, true);
    if (!StartPush(ResolveCardinalPushDirection(Hit.ImpactPoint, ContactDirection)))
    {
        BoxMesh->IgnoreActorWhenMoving(OtherActor, false);
    }
}

void ACMPushBox::ApplyEditorSettings()
{
    if (BoxMesh)
    {
        BoxMesh->SetGenerateOverlapEvents(true);
    }
    if (MechanismWeight)
    {
        MechanismWeight->MechanismWeight = FMath::Max(WeightInKg, 0.0f);
    }
}

FVector ACMPushBox::ResolveCardinalPushDirection(const FVector& ImpactPoint, const FVector& FallbackDirection) const
{
    FVector FaceDirection = BoxMesh ? BoxMesh->Bounds.Origin - ImpactPoint : FVector::ZeroVector;
    FaceDirection.Z = 0.0f;
    if (BoxMesh && !ImpactPoint.IsNearlyZero())
    {
        const FVector Extent = BoxMesh->Bounds.BoxExtent;
        const float XFaceRatio = FMath::Abs(FaceDirection.X) / FMath::Max(Extent.X, 1.0f);
        const float YFaceRatio = FMath::Abs(FaceDirection.Y) / FMath::Max(Extent.Y, 1.0f);
        if (!FaceDirection.IsNearlyZero())
        {
            return XFaceRatio >= YFaceRatio ? FVector(FMath::Sign(FaceDirection.X), 0.0f, 0.0f) : FVector(0.0f, FMath::Sign(FaceDirection.Y), 0.0f);
        }
    }

    FVector CardinalDirection = FallbackDirection;
    CardinalDirection.Z = 0.0f;
    if (FMath::Abs(CardinalDirection.X) >= FMath::Abs(CardinalDirection.Y))
    {
        CardinalDirection = FVector(FMath::Sign(CardinalDirection.X), 0.0f, 0.0f);
    }
    else
    {
        CardinalDirection = FVector(0.0f, FMath::Sign(CardinalDirection.Y), 0.0f);
    }
    return CardinalDirection;
}

bool ACMPushBox::StartPush(FVector WorldDirection)
{
    WorldDirection.Z = 0.0f;
    if (!WorldDirection.Normalize() || PushSpeed <= UE_SMALL_NUMBER || PushDistance <= UE_SMALL_NUMBER)
    {
        return false;
    }

    PushDirection = WorldDirection;
    RemainingPushDistance = PushDistance;
    if (!bIsMoving)
    {
        bIsMoving = true;
        OnRep_IsMoving();
    }
    return true;
}

void ACMPushBox::StopPush()
{
    BoxMesh->ClearMoveIgnoreActors();
    PushDirection = FVector::ZeroVector;
    RemainingPushDistance = 0.0f;
    if (bIsMoving)
    {
        bIsMoving = false;
        OnRep_IsMoving();
    }
}

void ACMPushBox::HandleSoundCatalogsRebuilt()
{
    RefreshMoveLoopSound();
}

void ACMPushBox::OnRep_IsMoving()
{
    RefreshMoveLoopSound();
}

void ACMPushBox::RefreshMoveLoopSound()
{
    if (!bIsMoving)
    {
        StopMoveLoopSound();
        return;
    }

    if (IsValid(MoveLoopSoundComponent) && MoveLoopSoundComponent->IsPlaying())
    {
        return;
    }

    MoveLoopSoundComponent = FCMSoundPlayback::PlayAttachedSFX(
        BoxMesh,
        CMSoundTags::Stage_Obstacle_PushBox_MoveLoop);
}

void ACMPushBox::StopMoveLoopSound()
{
    if (IsValid(MoveLoopSoundComponent))
    {
        MoveLoopSoundComponent->Stop();
        MoveLoopSoundComponent = nullptr;
    }
}
