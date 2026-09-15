#include "Parts/Tentacle/CMTentacleSegmentActor.h"

#include "Collision/CMCollisionChannels.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Parts/Core/CMDroppedPartActor.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Parts/Head/CMHeadPartActor.h"
#include "Player/CMChimera.h"
#include "Player/CMChimeraBodySegmentActor.h"
#include "Player/CMChimeraWrapTentacleComponent.h"
#include "Player/CMPartInterface.h"
#include "Player/CMPartSlotComponent.h"
#include "ProceduralMeshComponent.h"
#include "Sound/CMSoundPlayback.h"
#include "Sound/CMSoundTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraTentacle, Log, All);

const FName ACMTentacleSegmentActor::TentacleInteractiveActorTag(
    TEXT("TentacleInteractiveObject"));

ACMTentacleSegmentActor::ACMTentacleSegmentActor()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(false);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    DetectionSphere = CreateDefaultSubobject<USphereComponent>(
        TEXT("DetectionSphere"));
    DetectionSphere->SetupAttachment(SceneRoot);
    DetectionSphere->SetSphereRadius(DetectionRadius);
    DetectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    DetectionSphere->SetCollisionObjectType(ECC_WorldDynamic);
    DetectionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    DetectionSphere->SetCollisionResponseToChannel(
        ECC_WorldStatic, ECR_Overlap);
    DetectionSphere->SetCollisionResponseToChannel(
        ECC_WorldDynamic, ECR_Overlap);
    DetectionSphere->SetCollisionResponseToChannel(
        ECC_PhysicsBody, ECR_Overlap);
    DetectionSphere->SetCollisionResponseToChannel(
        CMCollision::ChimeraHurtbox, ECR_Overlap);
    DetectionSphere->SetGenerateOverlapEvents(true);
    DetectionSphere->SetCanEverAffectNavigation(false);

    GooBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GooBody"));
    GooBody->SetupAttachment(SceneRoot);
    GooBody->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.369f));
    GooBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GooBody->SetGenerateOverlapEvents(false);
    GooBody->SetCanEverAffectNavigation(false);

    SourceEffect = CreateDefaultSubobject<UNiagaraComponent>(
        TEXT("SourceEffect"));
    SourceEffect->SetupAttachment(SceneRoot);
    SourceEffect->SetAutoActivate(true);
    SourceEffect->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    GooDripsEffect = CreateDefaultSubobject<UNiagaraComponent>(
        TEXT("GooDripsEffect"));
    GooDripsEffect->SetupAttachment(SceneRoot);
    GooDripsEffect->SetAutoActivate(true);
    GooDripsEffect->SetCollisionEnabled(ECollisionEnabled::NoCollision);

}

void ACMTentacleSegmentActor::BeginPlay()
{
    Super::BeginPlay();

    DetectionSphere->SetSphereRadius(DetectionRadius);
    DetectionSphere->SetCollisionResponseToChannel(
        CMCollision::ChimeraHurtbox, ECR_Overlap);
    DetectionSphere->SetCollisionEnabled(
        HasAuthority()
            ? ECollisionEnabled::QueryOnly
            : ECollisionEnabled::NoCollision);
    if (GooBody)
    {
        GooBody->SetStaticMesh(GooBodyMesh);
        GooBody->SetMaterial(0, GooBodyMaterial);
        GooBody->SetVisibility(GooBodyMesh != nullptr, true);
    }
    if (SourceEffect)
    {
        if (!SourceEffect->GetAsset() && SourceNiagaraSystem)
        {
            SourceEffect->SetAsset(SourceNiagaraSystem);
        }
        if (SourceEffect->GetAsset())
        {
            SourceEffect->Activate(true);
        }
    }
    if (GooDripsEffect)
    {
        if (!GooDripsEffect->GetAsset() && GooDripsNiagaraSystem)
        {
            GooDripsEffect->SetAsset(GooDripsNiagaraSystem);
        }
        if (GooDripsEffect->GetAsset())
        {
            GooDripsEffect->Activate(true);
        }
    }
    ChimeraOwner = Cast<ACMChimera>(GetOwner());
    if (ACMChimeraBodySegmentActor* SegmentPresentation =
        Cast<ACMChimeraBodySegmentActor>(GetParentActor()))
    {
        SetSegmentActive(SegmentPresentation->IsSegmentActive());
        SegmentPresentation->RefreshIdleTentacleSource();
    }
    OnRep_VisualState();
}

void ACMTentacleSegmentActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (HasAuthority() && TentacleState == ECMTentacleState::Pulling)
    {
        AbortPull();
    }
    StopPullLoopSound();
    DestroyVisualComponents();
    DestroyMountedHeadTentacles();
    Super::EndPlay(EndPlayReason);
}

void ACMTentacleSegmentActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (HasAuthority())
    {
        if (!ChimeraOwner)
        {
            ChimeraOwner = Cast<ACMChimera>(GetOwner());
        }

        if (!ChimeraOwner || !ChimeraOwner->IsSegmentAlive(SegmentIndex))
        {
            if (TentacleState == ECMTentacleState::Pulling)
            {
                AbortPull();
            }
            SetTetheredActor(nullptr);
        }
        else if (TentacleState == ECMTentacleState::Pulling)
        {
            UpdatePull(DeltaTime);
        }
        else
        {
            RefreshOverlapTarget();
        }
    }

    UpdateVisual(DeltaTime);
    UpdateMountedHeadTentacles(DeltaTime);
}

void ACMTentacleSegmentActor::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACMTentacleSegmentActor, TetheredActor);
    DOREPLIFETIME(ACMTentacleSegmentActor, TentacleState);
    DOREPLIFETIME(ACMTentacleSegmentActor, VisualSeed);
    DOREPLIFETIME(ACMTentacleSegmentActor, SegmentIndex);
}

void ACMTentacleSegmentActor::InitializeForSegment(
    ACMChimera* InChimera,
    int32 InSegmentIndex,
    UPrimitiveComponent* InBodySegment,
    const bool bAttachToBodySegment)
{
    if (!InChimera || !InBodySegment)
    {
        return;
    }

    ChimeraOwner = InChimera;
    SegmentIndex = InSegmentIndex;
    if (HasAuthority())
    {
        SetOwner(InChimera);
    }
    if (bAttachToBodySegment)
    {
        AttachToComponent(
            InBodySegment,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    }
    SetActorRelativeLocation(SourceRelativeOffset);
    DetectionSphere->SetSphereRadius(DetectionRadius);
    if (HasAuthority())
    {
        ForceNetUpdate();
    }
}

void ACMTentacleSegmentActor::SetSegmentActive(const bool bInActive)
{
    if (!bInActive && HasAuthority())
    {
        if (TentacleState == ECMTentacleState::Pulling)
        {
            AbortPull();
        }
        SetTetheredActor(nullptr);
    }

    bSegmentActive = bInActive;
    SetActorTickEnabled(bInActive);
    SetActorHiddenInGame(!bInActive);
    if (!bInActive)
    {
        for (int32 PartSlotIndex = 0;
            PartSlotIndex < MountedHeadTentacles.Num();
            ++PartSlotIndex)
        {
            HideMountedHeadTentacle(PartSlotIndex);
        }
    }

    if (DetectionSphere)
    {
        DetectionSphere->SetCollisionEnabled(
            bInActive && HasAuthority()
                ? ECollisionEnabled::QueryOnly
                : ECollisionEnabled::NoCollision);
    }

    if (bInActive)
    {
        OnRep_VisualState();
    }
}

bool ACMTentacleSegmentActor::HasAttachablePart() const
{
    const ACMDroppedPartActor* DroppedPart =
        Cast<ACMDroppedPartActor>(TetheredActor);
    const ACMPartActorBase* UsablePart =
        Cast<ACMPartActorBase>(TetheredActor);
    return TentacleState == ECMTentacleState::Extended
        && ((DroppedPart
                && !DroppedPart->IsReservedForTentacle(this))
            || (UsablePart
                && !UsablePart->GetAttachedPartSlot()
                && !UsablePart->IsReservedForTentacle(this)));
}

int32 ACMTentacleSegmentActor::GetMountedHeadTentacleCount() const
{
    int32 VisibleCount = 0;
    for (const FCMMountedHeadTentacleRuntime& Runtime
        : MountedHeadTentacles)
    {
        VisibleCount += Runtime.TubeMesh
                && Runtime.TubeMesh->IsVisible()
                && !Runtime.TubeMesh->bHiddenInGame
            ? 1
            : 0;
    }
    return VisibleCount;
}

bool ACMTentacleSegmentActor::TryBeginPartAttachment(
    const FCMPartSlotAddress& PartSlotAddress)
{
    if (!HasAuthority()
        || TentacleState != ECMTentacleState::Extended
        || PartSlotAddress.SegmentIndex != SegmentIndex
        || !ChimeraOwner)
    {
        return false;
    }

    UCMPartSlotComponent* PartSlot =
        ChimeraOwner->GetPartSlotComponent(PartSlotAddress);
    ACMDroppedPartActor* DroppedPart =
        Cast<ACMDroppedPartActor>(TetheredActor);
    ACMPartActorBase* UsablePart =
        Cast<ACMPartActorBase>(TetheredActor);
    if (!PartSlot
        || PartSlot->HasAttachedPart()
        || (!DroppedPart && !UsablePart))
    {
        return false;
    }

    const bool bPullStarted = DroppedPart
        ? DroppedPart->TryReserveForTentacle(this)
            && DroppedPart->BeginTentaclePull(this)
        : UsablePart->TryReserveForTentacle(this);
    if (!bPullStarted)
    {
        if (DroppedPart)
        {
            DroppedPart->ReleaseTentacleReservation(this);
        }
        if (UsablePart)
        {
            UsablePart->ReleaseTentacleReservation(this);
        }
        return false;
    }

    PendingPartSlot = PartSlotAddress;
    PullStartTransform = TetheredActor->GetActorTransform();
    PullStartTransform.SetLocation(
        ResolveAuthoritativePickupLocation(TetheredActor));
    PullElapsedSeconds = 0.0f;
    TentacleState = ECMTentacleState::Pulling;
    ForceNetUpdate();
    RefreshPullLoopSound();

    UE_LOG(LogChimeraTentacle, Log,
        TEXT("[Tentacle Pull Started] Segment=%d Slot=%d Part=%s"),
        SegmentIndex,
        PartSlotAddress.PartSlotIndex,
        *GetNameSafe(TetheredActor));
    return true;
}

void ACMTentacleSegmentActor::RefreshOverlapTarget()
{
    if (!DetectionSphere)
    {
        SetTetheredActor(nullptr);
        return;
    }

    TArray<AActor*> OverlappingActors;
    DetectionSphere->GetOverlappingActors(OverlappingActors);

    AActor* NearestActor = nullptr;
    float NearestDistanceSquared = TNumericLimits<float>::Max();
    const FVector SourceLocation = GetActorLocation();
    for (AActor* Candidate : OverlappingActors)
    {
        if (!IsValid(Candidate)
            || Candidate == this
            || Candidate == ChimeraOwner
            || !Candidate->ActorHasTag(InteractiveActorTag))
        {
            continue;
        }

        const ACMDroppedPartActor* DroppedPart =
            Cast<ACMDroppedPartActor>(Candidate);
        if (DroppedPart && DroppedPart->IsReservedForTentacle(this))
        {
            continue;
        }

        const ACMPartActorBase* UsablePart =
            Cast<ACMPartActorBase>(Candidate);
        if (UsablePart && UsablePart->GetAttachedPartSlot())
        {
            continue;
        }
        if (UsablePart && UsablePart->IsReservedForTentacle(this))
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared(
            SourceLocation,
            ResolveAuthoritativePickupLocation(Candidate));
        if (DistanceSquared > FMath::Square(DetectionRadius))
        {
            continue;
        }
        if (DistanceSquared < NearestDistanceSquared)
        {
            NearestDistanceSquared = DistanceSquared;
            NearestActor = Candidate;
        }
    }

    SetTetheredActor(NearestActor);
}

void ACMTentacleSegmentActor::SetTetheredActor(AActor* NewTarget)
{
    if (TetheredActor == NewTarget)
    {
        return;
    }

    if (TetheredActor)
    {
        LastVisualTargetLocation = ResolveVisualTargetLocation(
            TetheredActor);
    }
    TetheredActor = NewTarget;
    TentacleState = TetheredActor
        ? ECMTentacleState::Extended
        : ECMTentacleState::Idle;
    VisualSeed = TetheredActor
        ? HashCombine(GetTypeHash(SegmentIndex), GetTypeHash(TetheredActor))
        : VisualSeed;
    ForceNetUpdate();
    OnRep_VisualState();
}

void ACMTentacleSegmentActor::UpdatePull(float DeltaTime)
{
    ACMDroppedPartActor* DroppedPart =
        Cast<ACMDroppedPartActor>(TetheredActor);
    ACMPartActorBase* UsablePart =
        Cast<ACMPartActorBase>(TetheredActor);
    AActor* PulledPart = TetheredActor;
    UCMPartSlotComponent* PartSlot = ChimeraOwner
        ? ChimeraOwner->GetPartSlotComponent(PendingPartSlot)
        : nullptr;
    const bool bReservationValid = DroppedPart
        ? DroppedPart->IsReservedByTentacle(this)
        : UsablePart && UsablePart->IsReservedByTentacle(this);
    if (!PulledPart
        || (!DroppedPart && !UsablePart)
        || !PartSlot
        || PartSlot->HasAttachedPart()
        || !bReservationValid)
    {
        AbortPull();
        return;
    }

    PullElapsedSeconds += DeltaTime;
    const float Alpha = FMath::Clamp(
        PullElapsedSeconds / FMath::Max(PullDuration, UE_SMALL_NUMBER),
        0.0f,
        1.0f);
    const float SmoothAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);
    const FTransform TargetTransform = PartSlot->GetComponentTransform();
    PulledPart->SetActorLocationAndRotation(
        FMath::Lerp(
            PullStartTransform.GetLocation(),
            TargetTransform.GetLocation(),
            SmoothAlpha),
        FQuat::Slerp(
            PullStartTransform.GetRotation(),
            TargetTransform.GetRotation(),
            SmoothAlpha));

    if (FVector::DistSquared(
            PulledPart->GetActorLocation(),
            TargetTransform.GetLocation())
        > FMath::Square(FMath::Max(
            AttachmentAcceptanceDistance, 0.0f)))
    {
        return;
    }

    const bool bAttached = DroppedPart
        ? DroppedPart->ConsumeIntoPartSlot(
            ChimeraOwner, PendingPartSlot)
        : ChimeraOwner->AttachPartToSlot(
            PendingPartSlot, UsablePart);
    if (DroppedPart && !bAttached)
    {
        DroppedPart->EndTentaclePull(this);
        DroppedPart->ReleaseTentacleReservation(this);
    }
    if (UsablePart)
    {
        UsablePart->ReleaseTentacleReservation(this);
    }

    UE_LOG(LogChimeraTentacle, Log,
        TEXT("[Tentacle Pull Finished] Segment=%d Slot=%d Result=%s"),
        SegmentIndex,
        PendingPartSlot.PartSlotIndex,
        bAttached ? TEXT("Attached") : TEXT("Rejected"));
    SetTetheredActor(nullptr);
    PendingPartSlot = FCMPartSlotAddress();
}

void ACMTentacleSegmentActor::AbortPull()
{
    if (ACMDroppedPartActor* DroppedPart =
        Cast<ACMDroppedPartActor>(TetheredActor))
    {
        DroppedPart->EndTentaclePull(this);
        DroppedPart->ReleaseTentacleReservation(this);
    }
    if (ACMPartActorBase* UsablePart =
        Cast<ACMPartActorBase>(TetheredActor))
    {
        UsablePart->ReleaseTentacleReservation(this);
    }
    SetTetheredActor(nullptr);
    PendingPartSlot = FCMPartSlotAddress();
}

void ACMTentacleSegmentActor::RefreshPullLoopSound()
{
    if (TentacleState != ECMTentacleState::Pulling || !IsValid(TetheredActor))
    {
        StopPullLoopSound();
        return;
    }
    if (IsValid(PullLoopSoundComponent) && PullLoopSoundComponent->IsPlaying())
    {
        return;
    }

    PullLoopSoundComponent = FCMSoundPlayback::PlayAttachedSFX(
        TetheredActor->GetRootComponent(),
        CMSoundTags::Part_AttachPullLoop);
}

void ACMTentacleSegmentActor::StopPullLoopSound()
{
    if (IsValid(PullLoopSoundComponent))
    {
        PullLoopSoundComponent->Stop();
        PullLoopSoundComponent = nullptr;
    }
}

void ACMTentacleSegmentActor::OnRep_VisualState()
{
    RefreshPullLoopSound();
    if (TetheredActor)
    {
        LastVisualTargetLocation = ResolveVisualTargetLocation(
            TetheredActor);
        EnsureVisualComponents();
    }
}

USkeletalMeshComponent* ACMTentacleSegmentActor::ResolveTargetPartMesh(
    AActor* Target) const
{
    if (ACMDroppedPartActor* DroppedPart =
        Cast<ACMDroppedPartActor>(Target))
    {
        return DroppedPart->GetPartMesh();
    }
    if (ACMPartActorBase* UsablePart = Cast<ACMPartActorBase>(Target))
    {
        return UsablePart->GetPartMesh();
    }
    return nullptr;
}

FVector ACMTentacleSegmentActor::ResolveAuthoritativePickupLocation(
    AActor* Target) const
{
    if (const ACMDroppedPartActor* DroppedPart =
        Cast<ACMDroppedPartActor>(Target))
    {
        return DroppedPart->GetAuthoritativePickupLocation();
    }
    if (const ACMPartActorBase* UsablePart =
        Cast<ACMPartActorBase>(Target))
    {
        return UsablePart->GetAuthoritativePickupLocation();
    }
    return Target ? Target->GetActorLocation() : FVector::ZeroVector;
}

FVector ACMTentacleSegmentActor::ResolveVisualTargetLocation(
    AActor* Target) const
{
    if (!IsValid(Target))
    {
        return LastVisualTargetLocation;
    }

    USkeletalMeshComponent* TargetMesh = ResolveTargetPartMesh(Target);
    if (!IsValid(TargetMesh))
    {
        return Target->GetActorLocation();
    }

    FVector ClosestPosition = TargetMesh->Bounds.Origin;
    FVector ClosestNormal = FVector::ZeroVector;
    FName ClosestBone = NAME_None;
    float ClosestDistance = 0.0f;
    if (TargetMesh->K2_GetClosestPointOnPhysicsAsset(
        GetActorLocation(),
        ClosestPosition,
        ClosestNormal,
        ClosestBone,
        ClosestDistance))
    {
        return ClosestPosition;
    }
    return TargetMesh->Bounds.Origin;
}

void ACMTentacleSegmentActor::EnsureVisualComponents()
{
    if (GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    if (!RuntimeSplineMesh)
    {
        RuntimeSplineMesh = NewObject<USplineMeshComponent>(
            this,
            MakeUniqueObjectName(
                this,
                USplineMeshComponent::StaticClass(),
                TEXT("RuntimeTentacleMesh")));
        if (!RuntimeSplineMesh)
        {
            return;
        }

        RuntimeSplineMesh->SetMobility(EComponentMobility::Movable);
        RuntimeSplineMesh->SetupAttachment(SceneRoot);
        RuntimeSplineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        RuntimeSplineMesh->SetGenerateOverlapEvents(false);
        RuntimeSplineMesh->SetCanEverAffectNavigation(false);
        RuntimeSplineMesh->SetForwardAxis(ESplineMeshAxis::X, false);
        if (!TentacleMeshVariants.IsEmpty())
        {
            RuntimeSplineMesh->SetStaticMesh(
                TentacleMeshVariants[0]);
        }
        if (TentacleMaterial)
        {
            RuntimeSplineMesh->SetMaterial(0, TentacleMaterial);
            RuntimeMaterial = RuntimeSplineMesh->CreateDynamicMaterialInstance(
                0, TentacleMaterial);
        }
        AddInstanceComponent(RuntimeSplineMesh);
        RuntimeSplineMesh->RegisterComponent();
        RuntimeSplineMesh->SetHiddenInGame(false);
        RuntimeSplineMesh->SetVisibility(true, true);
    }

    USceneComponent* TargetRoot = ResolveTargetPartMesh(TetheredActor);
    if (!TargetRoot && TetheredActor)
    {
        TargetRoot = TetheredActor->GetRootComponent();
    }
    if (TargetEffect
        && TargetEffect->GetAttachParent() != TargetRoot)
    {
        TargetEffect->Deactivate();
        TargetEffect->DestroyComponent();
        TargetEffect = nullptr;
    }
    if (!TargetEffect && TargetRoot && TargetNiagaraSystem)
    {
        TargetEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(
            TargetNiagaraSystem,
            TargetRoot,
            NAME_None,
            LastVisualTargetLocation,
            FRotator::ZeroRotator,
            EAttachLocation::KeepWorldPosition,
            false,
            true,
            ENCPoolMethod::None,
            true);
    }
}

void ACMTentacleSegmentActor::DestroyVisualComponents()
{
    if (RuntimeSplineMesh)
    {
        RuntimeSplineMesh->DestroyComponent();
        RuntimeSplineMesh = nullptr;
        RuntimeMaterial = nullptr;
    }
    if (TargetEffect)
    {
        TargetEffect->Deactivate();
        TargetEffect->DestroyComponent();
        TargetEffect = nullptr;
    }
}

void ACMTentacleSegmentActor::UpdateMountedHeadTentacles(
    const float DeltaTime)
{
    if (GetNetMode() == NM_DedicatedServer)
    {
        return;
    }
    if (!ChimeraOwner)
    {
        ChimeraOwner = Cast<ACMChimera>(GetOwner());
    }

    const int32 SlotCount = CMControl::PartSlotsPerSegment;
    MountedHeadTentacles.SetNum(SlotCount);
    MountedHeadTentacleElapsedSeconds += FMath::Max(DeltaTime, 0.0f);
    for (int32 PartSlotIndex = 0;
        PartSlotIndex < SlotCount;
        ++PartSlotIndex)
    {
        FCMPartSlotAddress SlotAddress;
        SlotAddress.SegmentIndex = SegmentIndex;
        SlotAddress.PartSlotIndex = PartSlotIndex;
        UCMPartSlotComponent* PartSlot = ChimeraOwner
            ? ChimeraOwner->GetPartSlotComponent(SlotAddress)
            : nullptr;
        AActor* AttachedPart = PartSlot
            ? PartSlot->GetAttachedPart()
            : nullptr;
        const bool bHasMountedHead = IsValid(AttachedPart)
            && (AttachedPart->IsA<ACMHeadPartActor>()
                || (AttachedPart->GetClass()->ImplementsInterface(
                        UCMPartInterface::StaticClass())
                    && ICMPartInterface::Execute_GetPartType(AttachedPart)
                        == ECMPartSlotType::Head));
        if (!bHasMountedHead)
        {
            HideMountedHeadTentacle(PartSlotIndex);
            continue;
        }

        EnsureMountedHeadTentacle(PartSlotIndex);
        FCMMountedHeadTentacleRuntime& Runtime =
            MountedHeadTentacles[PartSlotIndex];
        if (!Runtime.Spline)
        {
            continue;
        }

        USkeletalMeshComponent* HeadMesh = ResolveTargetPartMesh(
            AttachedPart);
        if (HeadMesh)
        {
            UpdateMountedHeadIdleAnimation(
                Runtime,
                *HeadMesh,
                PartSlotIndex);
        }
        FVector TargetWorld = FVector::ZeroVector;
        FName NeckBoneName = NAME_None;
        if (!HeadMesh
            || !ResolveMountedHeadNeckLocation(
                *PartSlot,
                *HeadMesh,
                TargetWorld,
                &NeckBoneName))
        {
            if (!Runtime.bLoggedConnectorFailure)
            {
                UE_LOG(LogChimeraTentacle, Warning,
                    TEXT("[Mounted Head Connector Missing] "
                        "Segment=%d Slot=%d Part=%s Mesh=%s Reason=NoNeckBone"),
                    SegmentIndex,
                    PartSlotIndex,
                    *GetNameSafe(AttachedPart),
                    *GetPathNameSafe(HeadMesh));
                Runtime.bLoggedConnectorFailure = true;
            }
            if (Runtime.TubeMesh)
            {
                Runtime.TubeMesh->SetVisibility(false, true);
                Runtime.TubeMesh->SetHiddenInGame(true, true);
            }
            continue;
        }
        const FVector SourceWorld = ResolveMountedHeadSourceLocation(
            TargetWorld);
        const FVector LocalSource = GetActorTransform()
            .InverseTransformPosition(SourceWorld);
        const FVector LocalTarget = GetActorTransform()
            .InverseTransformPosition(TargetWorld);
        const FVector LocalDelta = LocalTarget - LocalSource;
        const FVector LocalUp = GetActorTransform()
            .InverseTransformVectorNoScale(FVector::UpVector)
            .GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
        const FVector TargetDirection = LocalDelta.GetSafeNormal(
            UE_SMALL_NUMBER,
            FVector::ForwardVector);
        const FVector WaveAxis = FVector::CrossProduct(
            TargetDirection,
            LocalUp).GetSafeNormal(
                UE_SMALL_NUMBER,
                FVector::RightVector);
        const int32 PointCount = Runtime.Spline->GetNumberOfSplinePoints();
        for (int32 PointIndex = 0;
            PointIndex < PointCount;
            ++PointIndex)
        {
            const float Alpha = static_cast<float>(PointIndex)
                / static_cast<float>(FMath::Max(PointCount - 1, 1));
            const float Envelope = FMath::Sin(PI * Alpha);
            const float WavePhase = MountedHeadTentacleElapsedSeconds
                    * MountedHeadWaveSpeed
                + Alpha * 2.0f * PI
                + static_cast<float>(PartSlotIndex) * PI;
            const FVector Point = LocalSource + LocalDelta * Alpha
                + LocalUp
                    * MountedHeadTentacleSag
                    * 4.0f
                    * Alpha
                    * (1.0f - Alpha)
                + WaveAxis
                    * FMath::Sin(WavePhase)
                    * MountedHeadWaveAmplitude
                    * Envelope;
            Runtime.Spline->SetLocationAtSplinePoint(
                PointIndex,
                Point,
                ESplineCoordinateSpace::Local,
                false);
        }
        Runtime.Spline->UpdateSpline();

        if (Runtime.TubeMesh)
        {
            CMChimeraWrapTentacle::UpdateTubeMeshFromSpline(
                *Runtime.Spline,
                *Runtime.TubeMesh,
                MountedHeadTentacleWidth);
            const FProcMeshSection* MeshSection =
                Runtime.TubeMesh->GetProcMeshSection(0);
            const bool bConnectorRenderable = MeshSection
                && MeshSection->ProcVertexBuffer.Num() > 0
                && FVector::DistSquared(SourceWorld, TargetWorld) > 1.0f
                && Runtime.TubeMesh->IsVisible()
                && !Runtime.TubeMesh->bHiddenInGame;
            if (!bConnectorRenderable && !Runtime.bLoggedConnectorFailure)
            {
                UE_LOG(LogChimeraTentacle, Warning,
                    TEXT("[Mounted Head Connector Missing] "
                        "Segment=%d Slot=%d Part=%s Bone=%s "
                        "Distance=%.1f Vertices=%d Visible=%s Hidden=%s"),
                    SegmentIndex,
                    PartSlotIndex,
                    *GetNameSafe(AttachedPart),
                    *NeckBoneName.ToString(),
                    FVector::Distance(SourceWorld, TargetWorld),
                    MeshSection ? MeshSection->ProcVertexBuffer.Num() : 0,
                    Runtime.TubeMesh->IsVisible()
                        ? TEXT("true") : TEXT("false"),
                    Runtime.TubeMesh->bHiddenInGame
                        ? TEXT("true") : TEXT("false"));
                Runtime.bLoggedConnectorFailure = true;
            }
            else if (bConnectorRenderable && !Runtime.bLoggedConnectorReady)
            {
                UE_LOG(LogChimeraTentacle, Display,
                    TEXT("[Mounted Head Connector Ready] "
                        "Segment=%d Slot=%d Part=%s Bone=%s "
                        "Source=%s Target=%s Distance=%.1f Vertices=%d"),
                    SegmentIndex,
                    PartSlotIndex,
                    *GetNameSafe(AttachedPart),
                    *NeckBoneName.ToString(),
                    *SourceWorld.ToCompactString(),
                    *TargetWorld.ToCompactString(),
                    FVector::Distance(SourceWorld, TargetWorld),
                    MeshSection->ProcVertexBuffer.Num());
                Runtime.bLoggedConnectorReady = true;
            }
        }
    }
}

void ACMTentacleSegmentActor::UpdateMountedHeadIdleAnimation(
    FCMMountedHeadTentacleRuntime& Runtime,
    USkeletalMeshComponent& HeadMesh,
    const int32 PartSlotIndex)
{
    if (Runtime.AnimatedHeadMesh != &HeadMesh)
    {
        RestoreMountedHeadIdleAnimation(Runtime);
        Runtime.AnimatedHeadMesh = &HeadMesh;
        Runtime.HeadMeshBaseRelativeTransform =
            HeadMesh.GetRelativeTransform();
        Runtime.bHasHeadMeshBaseTransform = true;
        Runtime.bLoggedConnectorFailure = false;
        Runtime.bLoggedConnectorReady = false;
    }
    if (!Runtime.bHasHeadMeshBaseTransform)
    {
        return;
    }

    const float Phase = MountedHeadTentacleElapsedSeconds
            * MountedHeadIdleSpeed
        + static_cast<float>(PartSlotIndex) * PI;
    const FTransform ParentTransform = HeadMesh.GetAttachParent()
        ? HeadMesh.GetAttachParent()->GetComponentTransform()
        : FTransform::Identity;
    const FVector ParentUp = ParentTransform
        .InverseTransformVectorNoScale(FVector::UpVector)
        .GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
    const FVector ParentRight = ParentTransform
        .InverseTransformVectorNoScale(FVector::RightVector)
        .GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
    const FVector ParentForward = ParentTransform
        .InverseTransformVectorNoScale(FVector::ForwardVector)
        .GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
    const FVector PositionOffset = ParentUp
            * (MountedHeadIdleHeightOffset
                + FMath::Sin(Phase)
                    * MountedHeadIdleVerticalAmplitude)
        + ParentRight
            * FMath::Sin(Phase * 0.79f)
            * MountedHeadIdleHorizontalAmplitude
        + ParentForward
            * FMath::Sin(Phase * 0.61f)
            * MountedHeadIdleHorizontalAmplitude
            * 0.35f;
    FTransform AnimatedTransform = Runtime.HeadMeshBaseRelativeTransform;
    AnimatedTransform.SetLocation(
        Runtime.HeadMeshBaseRelativeTransform.GetLocation()
        + PositionOffset);
    HeadMesh.SetRelativeTransform(
        AnimatedTransform,
        false,
        nullptr,
        ETeleportType::TeleportPhysics);
    HeadMesh.UpdateBounds();
}

void ACMTentacleSegmentActor::RestoreMountedHeadIdleAnimation(
    FCMMountedHeadTentacleRuntime& Runtime)
{
    if (Runtime.bHasHeadMeshBaseTransform
        && IsValid(Runtime.AnimatedHeadMesh))
    {
        Runtime.AnimatedHeadMesh->SetRelativeTransform(
            Runtime.HeadMeshBaseRelativeTransform,
            false,
            nullptr,
            ETeleportType::TeleportPhysics);
        Runtime.AnimatedHeadMesh->UpdateBounds();
    }
    Runtime.AnimatedHeadMesh = nullptr;
    Runtime.HeadMeshBaseRelativeTransform = FTransform::Identity;
    Runtime.bHasHeadMeshBaseTransform = false;
}

void ACMTentacleSegmentActor::EnsureMountedHeadTentacle(
    const int32 PartSlotIndex)
{
    if (!MountedHeadTentacles.IsValidIndex(PartSlotIndex))
    {
        return;
    }

    FCMMountedHeadTentacleRuntime& Runtime =
        MountedHeadTentacles[PartSlotIndex];
    if (!Runtime.Spline)
    {
        const int32 PointCount = FMath::Max(MountedHeadSplinePointCount, 2);
        Runtime.Spline = NewObject<USplineComponent>(
            this,
            MakeUniqueObjectName(
                this,
                USplineComponent::StaticClass(),
                TEXT("MountedHeadTentacleSpline")));
        Runtime.Spline->SetMobility(EComponentMobility::Movable);
        Runtime.Spline->SetupAttachment(SceneRoot);
        AddInstanceComponent(Runtime.Spline);
        Runtime.Spline->RegisterComponent();
        Runtime.Spline->ClearSplinePoints(false);
        for (int32 PointIndex = 0;
            PointIndex < PointCount;
            ++PointIndex)
        {
            Runtime.Spline->AddSplinePoint(
                FVector::ZeroVector,
                ESplineCoordinateSpace::Local,
                false);
            Runtime.Spline->SetSplinePointType(
                PointIndex,
                ESplinePointType::Curve,
                false);
        }
        Runtime.Spline->UpdateSpline();
    }
    if (Runtime.TubeMesh)
    {
        return;
    }

    Runtime.TubeMesh = NewObject<UProceduralMeshComponent>(
        this,
        MakeUniqueObjectName(
            this,
            UProceduralMeshComponent::StaticClass(),
            TEXT("MountedHeadConnectorTube")));
    Runtime.TubeMesh->SetMobility(EComponentMobility::Movable);
    Runtime.TubeMesh->SetupAttachment(SceneRoot);
    Runtime.TubeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Runtime.TubeMesh->SetGenerateOverlapEvents(false);
    Runtime.TubeMesh->SetCanEverAffectNavigation(false);
    Runtime.TubeMesh->SetCastShadow(false);
    Runtime.TubeMesh->bUseAttachParentBound = false;
    Runtime.TubeMesh->SetBoundsScale(4.0f);
    Runtime.TubeMesh->SetCullDistance(0.0f);
    Runtime.TubeMesh->SetRenderInMainPass(true);
    Runtime.TubeMesh->SetTranslucentSortPriority(10);
    UMaterialInterface* RenderMaterial = TentacleMaterial
        && TentacleMaterial->CheckMaterialUsage_Concurrent(
            MATUSAGE_StaticMesh)
        ? TentacleMaterial.Get()
        : UMaterial::GetDefaultMaterial(MD_Surface);
    Runtime.TubeMesh->SetMaterial(0, RenderMaterial);
    Runtime.TubeMesh->SetVisibility(false, true);
    Runtime.TubeMesh->SetHiddenInGame(true, true);
    AddInstanceComponent(Runtime.TubeMesh);
    Runtime.TubeMesh->RegisterComponent();
    Runtime.TubeMaterial = Runtime.TubeMesh->CreateDynamicMaterialInstance(
        0,
        RenderMaterial);
    if (Runtime.TubeMaterial && !CollapseMaterialParameter.IsNone())
    {
        Runtime.TubeMaterial->SetScalarParameterValue(
            CollapseMaterialParameter,
            0.0f);
    }
}

void ACMTentacleSegmentActor::HideMountedHeadTentacle(
    const int32 PartSlotIndex)
{
    if (!MountedHeadTentacles.IsValidIndex(PartSlotIndex))
    {
        return;
    }
    RestoreMountedHeadIdleAnimation(
        MountedHeadTentacles[PartSlotIndex]);
    if (UProceduralMeshComponent* TubeMesh =
        MountedHeadTentacles[PartSlotIndex].TubeMesh)
    {
        TubeMesh->SetVisibility(false, true);
        TubeMesh->SetHiddenInGame(true, true);
    }
}

void ACMTentacleSegmentActor::DestroyMountedHeadTentacles()
{
    for (FCMMountedHeadTentacleRuntime& Runtime : MountedHeadTentacles)
    {
        RestoreMountedHeadIdleAnimation(Runtime);
        if (Runtime.TubeMesh)
        {
            Runtime.TubeMesh->DestroyComponent();
        }
        if (Runtime.Spline)
        {
            Runtime.Spline->DestroyComponent();
        }
    }
    MountedHeadTentacles.Reset();
}

bool ACMTentacleSegmentActor::ResolveMountedHeadNeckLocation(
    const UCMPartSlotComponent& PartSlot,
    const USkeletalMeshComponent& HeadMesh,
    FVector& OutWorldLocation,
    FName* OutBoneName) const
{
    FName NeckBoneName = NAME_None;
    if (!PartSlot.ResolveHeadMountBoneName(HeadMesh, NeckBoneName))
    {
        return false;
    }
    const int32 BoneIndex = HeadMesh.GetBoneIndex(NeckBoneName);
    if (BoneIndex == INDEX_NONE)
    {
        return false;
    }
    OutWorldLocation = HeadMesh.GetBoneLocation(
        NeckBoneName,
        EBoneSpaces::WorldSpace);
    if (OutBoneName)
    {
        *OutBoneName = NeckBoneName;
    }
    return true;
}

FVector ACMTentacleSegmentActor::ResolveMountedHeadSourceLocation(
    const FVector&) const
{
    return GetActorLocation();
}

void ACMTentacleSegmentActor::UpdateVisual(float DeltaTime)
{
    if (GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    const float TargetAlpha = TetheredActor ? 1.0f : 0.0f;
    const float Duration = TetheredActor
        ? ExtensionDuration
        : RetractionDuration;
    VisualAlpha = FMath::FInterpConstantTo(
        VisualAlpha,
        TargetAlpha,
        DeltaTime,
        1.0f / FMath::Max(Duration, UE_SMALL_NUMBER));

    if (TetheredActor)
    {
        LastVisualTargetLocation = ResolveVisualTargetLocation(
            TetheredActor);
        EnsureVisualComponents();
        if (TargetEffect)
        {
            TargetEffect->SetWorldLocation(LastVisualTargetLocation);
        }
    }
    if (!RuntimeSplineMesh)
    {
        return;
    }

    const FVector LocalTarget = GetActorTransform().InverseTransformPosition(
        LastVisualTargetLocation) * VisualAlpha;
    FRandomStream RandomStream(VisualSeed);
    const FVector Noise(
        0.0f,
        RandomStream.FRandRange(-TangentNoise, TangentNoise),
        RandomStream.FRandRange(-TangentNoise, TangentNoise));
    const FVector Tangent = LocalTarget * 0.5f + Noise;
    RuntimeSplineMesh->SetStartScale(
        FVector2D(TetheredTentacleWidth),
        false);
    RuntimeSplineMesh->SetEndScale(
        FVector2D(TetheredTentacleWidth),
        false);
    RuntimeSplineMesh->SetStartAndEnd(
        FVector::ZeroVector,
        Tangent,
        LocalTarget,
        Tangent,
        false);
    RuntimeSplineMesh->UpdateMesh();
    RuntimeSplineMesh->UpdateBounds();
    RuntimeSplineMesh->MarkRenderTransformDirty();
    if (RuntimeMaterial && !CollapseMaterialParameter.IsNone())
    {
        RuntimeMaterial->SetScalarParameterValue(
            CollapseMaterialParameter,
            FMath::Lerp(-1.0f, 0.0f, VisualAlpha));
    }

    if (!TetheredActor && VisualAlpha <= UE_KINDA_SMALL_NUMBER)
    {
        DestroyVisualComponents();
    }
}
