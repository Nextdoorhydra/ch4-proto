#include "Parts/Tentacle/CMTentacleSegmentActor.h"

#include "Collision/CMCollisionChannels.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Parts/Core/CMDroppedPartActor.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Player/CMChimera.h"
#include "Player/CMPartSlotComponent.h"

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
    OnRep_VisualState();
}

void ACMTentacleSegmentActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (HasAuthority() && TentacleState == ECMTentacleState::Pulling)
    {
        AbortPull();
    }
    DestroyVisualComponents();
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
    UPrimitiveComponent* InBodySegment)
{
    if (!HasAuthority() || !InChimera || !InBodySegment)
    {
        return;
    }

    ChimeraOwner = InChimera;
    SegmentIndex = InSegmentIndex;
    SetOwner(InChimera);
    AttachToComponent(
        InBodySegment,
        FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    SetActorRelativeLocation(SourceRelativeOffset);
    DetectionSphere->SetSphereRadius(DetectionRadius);
    ForceNetUpdate();
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
    PullElapsedSeconds = 0.0f;
    TentacleState = ECMTentacleState::Pulling;
    ForceNetUpdate();

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
            SourceLocation, Candidate->GetActorLocation());
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
        LastVisualTargetLocation = TetheredActor->GetActorLocation();
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

void ACMTentacleSegmentActor::OnRep_VisualState()
{
    if (TetheredActor)
    {
        LastVisualTargetLocation = TetheredActor->GetActorLocation();
        EnsureVisualComponents();
    }
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

    USceneComponent* TargetRoot = TetheredActor
        ? TetheredActor->GetRootComponent()
        : nullptr;
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
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            EAttachLocation::KeepRelativeOffset,
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
        LastVisualTargetLocation = TetheredActor->GetActorLocation();
        EnsureVisualComponents();
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
    RuntimeSplineMesh->SetStartAndEnd(
        FVector::ZeroVector,
        Tangent,
        LocalTarget,
        Tangent,
        true);
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
