#include "Stage/Obstacle/CMConveyorDistributorActor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Stage/Obstacle/CMConveyorSplineActor.h"
#include "Stage/Obstacle/CMConveyorTransportTags.h"
#include "TimerManager.h"

ACMConveyorDistributorActor::ACMConveyorDistributorActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    LeftMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftMesh"));
    LeftMesh->SetupAttachment(SceneRoot);
    LeftMesh->SetMobility(EComponentMobility::Movable);
    LeftMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    LeftMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

    RightMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightMesh"));
    RightMesh->SetupAttachment(SceneRoot);
    RightMesh->SetMobility(EComponentMobility::Movable);
    RightMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    RightMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

    LeftPath = CreateDefaultSubobject<USplineComponent>(TEXT("LeftPath"));
    LeftPath->SetupAttachment(SceneRoot);
    LeftPath->SetMobility(EComponentMobility::Movable);
    LeftPath->SetDrawDebug(true);
    LeftPath->SetHiddenInGame(false);
    LeftPath->bEditableWhenInherited = true;

    RightPath = CreateDefaultSubobject<USplineComponent>(TEXT("RightPath"));
    RightPath->SetupAttachment(SceneRoot);
    RightPath->SetMobility(EComponentMobility::Movable);
    RightPath->SetDrawDebug(true);
    RightPath->SetHiddenInGame(false);
    RightPath->bEditableWhenInherited = true;

#if WITH_EDITORONLY_DATA
    LeftPath->EditorUnselectedSplineSegmentColor = FLinearColor(0.0f, 0.8f, 0.2f);
    LeftPath->EditorSelectedSplineSegmentColor = FLinearColor::Yellow;
    RightPath->EditorUnselectedSplineSegmentColor = FLinearColor(0.1f, 0.45f, 1.0f);
    RightPath->EditorSelectedSplineSegmentColor = FLinearColor::Yellow;
#endif

    LeftDetection = CreateDefaultSubobject<UBoxComponent>(TEXT("LeftDetection"));
    LeftDetection->SetupAttachment(SceneRoot);
    ConfigureDetection(LeftDetection);

    RightDetection = CreateDefaultSubobject<UBoxComponent>(TEXT("RightDetection"));
    RightDetection->SetupAttachment(SceneRoot);
    ConfigureDetection(RightDetection);

    RebuildLayout();
}

void ACMConveyorDistributorActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RebuildLayout();
}

void ACMConveyorDistributorActor::BeginPlay()
{
    Super::BeginPlay();
    RebuildLayout();

    LeftDetection->OnComponentBeginOverlap.RemoveAll(this);
    LeftDetection->OnComponentEndOverlap.RemoveAll(this);
    LeftDetection->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleDetectionBeginOverlap);
    LeftDetection->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::HandleDetectionEndOverlap);
    RightDetection->OnComponentBeginOverlap.RemoveAll(this);
    RightDetection->OnComponentEndOverlap.RemoveAll(this);
    RightDetection->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleDetectionBeginOverlap);
    RightDetection->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::HandleDetectionEndOverlap);

    const ECollisionEnabled::Type DetectionCollision = HasAuthority() ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision;
    LeftDetection->SetCollisionEnabled(DetectionCollision);
    RightDetection->SetCollisionEnabled(DetectionCollision);
    if (HasAuthority())
    {
        ScanInitialParts();
    }
    UpdateMovementTimer();
}

void ACMConveyorDistributorActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopMovementTimer();
    LeftDetection->OnComponentBeginOverlap.RemoveAll(this);
    LeftDetection->OnComponentEndOverlap.RemoveAll(this);
    RightDetection->OnComponentBeginOverlap.RemoveAll(this);
    RightDetection->OnComponentEndOverlap.RemoveAll(this);
    for (const TPair<TWeakObjectPtr<ACMPartActorBase>, FTrackedPartState>& Pair : TrackedParts)
    {
        if (ACMPartActorBase* PartActor = Pair.Key.Get())
        {
            PartActor->Tags.Remove(CMConveyorTransportTags::Transporting);
        }
    }
    TrackedParts.Empty();
    Super::EndPlay(EndPlayReason);
}

void ACMConveyorDistributorActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ThisClass, Direction);
    DOREPLIFETIME(ThisClass, RoutingMode);
    DOREPLIFETIME(ThisClass, bSideMeshesSwapped);
}

void ACMConveyorDistributorActor::SetDistributorDirection(ECMConveyorDistributorDirection NewDirection)
{
    if (!HasAuthority() || Direction == NewDirection)
    {
        return;
    }

    Direction = NewDirection;
    LastMovementUpdateTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    OnDirectionChanged.Broadcast(Direction);
    ForceNetUpdate();
}

void ACMConveyorDistributorActor::ReverseDistributorDirection()
{
    SetDistributorDirection(Direction == ECMConveyorDistributorDirection::Forward
        ? ECMConveyorDistributorDirection::Reverse : ECMConveyorDistributorDirection::Forward);
}

void ACMConveyorDistributorActor::SetRoutingMode(ECMConveyorDistributorRoutingMode NewRoutingMode)
{
    if (!HasAuthority() || RoutingMode == NewRoutingMode)
    {
        return;
    }

    RoutingMode = NewRoutingMode;
    OnRoutingModeChanged.Broadcast(RoutingMode);
    ForceNetUpdate();
}

void ACMConveyorDistributorActor::ToggleRoutingMode()
{
    SetRoutingMode(RoutingMode == ECMConveyorDistributorRoutingMode::Straight
        ? ECMConveyorDistributorRoutingMode::Cross : ECMConveyorDistributorRoutingMode::Straight);
}

void ACMConveyorDistributorActor::SetSideMeshesSwapped(bool bNewSwapped)
{
    if (!HasAuthority() || bSideMeshesSwapped == bNewSwapped)
    {
        return;
    }

    bSideMeshesSwapped = bNewSwapped;
    RebuildLayout();
    ForceNetUpdate();
}

void ACMConveyorDistributorActor::SwapSideMeshes()
{
    SetSideMeshesSwapped(!bSideMeshesSwapped);
}

void ACMConveyorDistributorActor::GetConnectionWorldTransforms(TArray<FTransform>& OutTransforms) const
{
    OutTransforms.Reset();
    for (const USplineComponent* Path : { LeftPath.Get(), RightPath.Get() })
    {
        if (!Path || Path->GetNumberOfSplinePoints() < 2)
        {
            continue;
        }

        OutTransforms.Add(Path->GetTransformAtSplinePoint(0, ESplineCoordinateSpace::World, true));
        OutTransforms.Add(Path->GetTransformAtSplinePoint(1, ESplineCoordinateSpace::World, true));
    }
}

bool ACMConveyorDistributorActor::TryAcceptPart(ACMPartActorBase* PartActor)
{
    if (!HasAuthority() || !IsPartInsideDetection(PartActor))
    {
        return false;
    }

    RegisterPart(PartActor);
    return TrackedParts.Contains(PartActor);
}

void ACMConveyorDistributorActor::HandleDetectionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (HasAuthority())
    {
        RegisterPart(Cast<ACMPartActorBase>(OtherActor));
    }
}

void ACMConveyorDistributorActor::HandleDetectionEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
    if (!HasAuthority())
    {
        return;
    }

    if (ACMPartActorBase* PartActor = Cast<ACMPartActorBase>(OtherActor))
    {
        if (FTrackedPartState* State = TrackedParts.Find(PartActor))
        {
            State->OverlapCount = FMath::Max(State->OverlapCount - 1, 0);
        }
    }
}

void ACMConveyorDistributorActor::OnRep_Direction()
{
    OnDirectionChanged.Broadcast(Direction);
}

void ACMConveyorDistributorActor::OnRep_RoutingMode()
{
    OnRoutingModeChanged.Broadcast(RoutingMode);
}

void ACMConveyorDistributorActor::OnRep_SideMeshesSwapped()
{
    RebuildLayout();
}

void ACMConveyorDistributorActor::RebuildLayout()
{
    UStaticMesh* LeftAsset = bSideMeshesSwapped ? AssemblyLineBox06 : AssemblyLineBox04;
    UStaticMesh* RightAsset = bSideMeshesSwapped ? AssemblyLineBox04 : AssemblyLineBox06;
    ConfigureLane(LeftMesh, LeftPath, LeftDetection, LeftAsset, true);
    ConfigureLane(RightMesh, RightPath, RightDetection, RightAsset, false);
}

void ACMConveyorDistributorActor::ConfigureLane(UStaticMeshComponent* MeshComponent, USplineComponent* Path, UBoxComponent* Detection, UStaticMesh* MeshAsset, bool bLeftLane)
{
    if (!MeshComponent || !Path || !Detection)
    {
        return;
    }

    const float LaneCenterY = (bLeftLane ? -1.0f : 1.0f) * FMath::Max(LaneSpacing, 10.0f) * 0.5f;
    const float HalfLength = FMath::Max(DistributorLength, 10.0f) * 0.5f;
    const bool bSmallMesh = MeshAsset && MeshAsset == AssemblyLineBox06;
    const float MeshX = bSmallMesh ? (bSideMeshesSwapped ? 1.0f : -1.0f) * FMath::Max(SmallMeshOutputOffset, 0.0f) : 0.0f;
    const FRotator MeshRotation = bSideMeshesSwapped ? FRotator(0.0f, 180.0f, 0.0f) : FRotator::ZeroRotator;

    MeshComponent->SetStaticMesh(MeshAsset);
    MeshComponent->SetRelativeLocation(FVector(MeshX, LaneCenterY, 0.0f));
    MeshComponent->SetRelativeRotation(MeshRotation);
    MeshComponent->SetRelativeScale3D(FVector::OneVector);

    Path->SetAbsolute(false, false, false);
    Path->SetRelativeTransform(FTransform::Identity);
    Path->ClearSplinePoints(false);
    Path->AddSplinePoint(FVector(-HalfLength, LaneCenterY, PathHeight), ESplineCoordinateSpace::Local, false);
    Path->AddSplinePoint(FVector(HalfLength, LaneCenterY, PathHeight), ESplineCoordinateSpace::Local, false);
    Path->SetSplinePointType(0, ESplinePointType::Linear, false);
    Path->SetSplinePointType(1, ESplinePointType::Linear, false);
    Path->SetClosedLoop(false, false);
    Path->UpdateSpline();

    const float SafeHeight = FMath::Max(DetectionHeight, 1.0f);
    Detection->SetRelativeLocation(FVector(0.0f, LaneCenterY, PathHeight + SafeHeight * 0.5f));
    Detection->SetRelativeRotation(FRotator::ZeroRotator);
    Detection->SetBoxExtent(FVector(HalfLength + FMath::Max(DetectionEndPadding, 0.0f), FMath::Max(DetectionHalfWidth, 1.0f), SafeHeight * 0.5f));
}

void ACMConveyorDistributorActor::ConfigureDetection(UBoxComponent* Detection)
{
    Detection->SetMobility(EComponentMobility::Movable);
    Detection->SetCollisionObjectType(ECC_WorldDynamic);
    Detection->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Detection->SetCollisionResponseToAllChannels(ECR_Overlap);
    Detection->SetGenerateOverlapEvents(true);
    Detection->SetCanEverAffectNavigation(false);
    Detection->SetHiddenInGame(true);
}

void ACMConveyorDistributorActor::ScanInitialParts()
{
    TMap<TWeakObjectPtr<ACMPartActorBase>, int32> InitialOverlapCounts;
    for (UBoxComponent* Detection : { LeftDetection.Get(), RightDetection.Get() })
    {
        TArray<AActor*> OverlappingActors;
        Detection->GetOverlappingActors(OverlappingActors, ACMPartActorBase::StaticClass());
        for (AActor* OverlappingActor : OverlappingActors)
        {
            if (ACMPartActorBase* PartActor = Cast<ACMPartActorBase>(OverlappingActor))
            {
                ++InitialOverlapCounts.FindOrAdd(PartActor);
            }
        }
    }

    for (const TPair<TWeakObjectPtr<ACMPartActorBase>, int32>& Pair : InitialOverlapCounts)
    {
        RegisterPart(Pair.Key.Get(), Pair.Value);
    }
}

void ACMConveyorDistributorActor::RegisterPart(ACMPartActorBase* PartActor, int32 AddedOverlapCount)
{
    if (!HasAuthority() || !IsValid(PartActor) || PartActor->IsAttached()
        || PartActor->GetAttachParentActor())
    {
        return;
    }

    const TWeakObjectPtr<ACMPartActorBase> PartKey(PartActor);
    if (FTrackedPartState* ExistingState = TrackedParts.Find(PartKey))
    {
        ExistingState->OverlapCount += FMath::Max(AddedOverlapCount, 1);
        return;
    }
    if (PartActor->ActorHasTag(CMConveyorTransportTags::Transporting))
    {
        return;
    }

    FTrackedPartState& State = TrackedParts.Add(PartKey);
    State.Lane = FindClosestLane(PartActor->GetActorLocation(), State.Progress);
    State.OverlapCount = FMath::Max(AddedOverlapCount, 1);
    PartActor->Tags.AddUnique(CMConveyorTransportTags::Transporting);
    if (bPreserveInitialPartPlacement)
    {
        if (USplineComponent* Path = GetLanePath(State.Lane))
        {
            const float PathDistance = State.Progress * Path->GetSplineLength();
            const FTransform PathTransform = Path->GetTransformAtDistanceAlongSpline(PathDistance, ESplineCoordinateSpace::World, true);
            State.InitialPathLocalOffset = PathTransform.InverseTransformPosition(PartActor->GetActorLocation());
            State.InitialPathRelativeRotation = PathTransform.GetRotation().Inverse() * PartActor->GetActorQuat();
            State.InitialPathRelativeRotation.Normalize();
            State.bHasInitialPlacement = true;
        }
    }
    ApplyPartTransform(PartActor, State);
    UpdateMovementTimer();
}

bool ACMConveyorDistributorActor::IsPartInsideDetection(const ACMPartActorBase* PartActor) const
{
    if (!IsValid(PartActor))
    {
        return false;
    }

    for (const UBoxComponent* Detection : { LeftDetection.Get(), RightDetection.Get() })
    {
        const FVector LocalLocation = Detection->GetComponentTransform().InverseTransformPosition(PartActor->GetActorLocation());
        const FVector BoxExtent = Detection->GetUnscaledBoxExtent();
        const bool bActorOriginInside = FMath::Abs(LocalLocation.X) <= BoxExtent.X && FMath::Abs(LocalLocation.Y) <= BoxExtent.Y
            && FMath::Abs(LocalLocation.Z) <= BoxExtent.Z;
        if (Detection->IsOverlappingActor(PartActor) || bActorOriginInside)
        {
            return true;
        }
    }
    return false;
}

ECMConveyorDistributorLane ACMConveyorDistributorActor::FindClosestLane(const FVector& WorldLocation, float& OutProgress) const
{
    ECMConveyorDistributorLane ClosestLane = ECMConveyorDistributorLane::Left;
    float ClosestDistanceSquared = TNumericLimits<float>::Max();
    OutProgress = 0.0f;
    for (const ECMConveyorDistributorLane Lane : { ECMConveyorDistributorLane::Left, ECMConveyorDistributorLane::Right })
    {
        USplineComponent* Path = GetLanePath(Lane);
        if (!Path)
        {
            continue;
        }

        const float InputKey = Path->FindInputKeyClosestToWorldLocation(WorldLocation);
        const FVector ClosestLocation = Path->GetLocationAtSplineInputKey(InputKey, ESplineCoordinateSpace::World);
        const float DistanceSquared = FVector::DistSquared(WorldLocation, ClosestLocation);
        if (DistanceSquared < ClosestDistanceSquared)
        {
            ClosestDistanceSquared = DistanceSquared;
            ClosestLane = Lane;
            const float PathLength = Path->GetSplineLength();
            const float PathDistance = Path->GetDistanceAlongSplineAtSplineInputKey(InputKey);
            OutProgress = PathLength > UE_KINDA_SMALL_NUMBER ? FMath::Clamp(PathDistance / PathLength, 0.0f, 1.0f) : 0.0f;
        }
    }
    return ClosestLane;
}

USplineComponent* ACMConveyorDistributorActor::GetLanePath(ECMConveyorDistributorLane Lane) const
{
    return Lane == ECMConveyorDistributorLane::Left ? LeftPath : RightPath;
}

void ACMConveyorDistributorActor::ApplyPartTransform(ACMPartActorBase* PartActor, const FTrackedPartState& State) const
{
    USplineComponent* Path = GetLanePath(State.Lane);
    if (!IsValid(PartActor) || !Path)
    {
        return;
    }

    const float PathDistance = State.Progress * Path->GetSplineLength();
    const FTransform PathTransform = Path->GetTransformAtDistanceAlongSpline(PathDistance, ESplineCoordinateSpace::World, true);
    FVector TargetLocation = PathTransform.GetLocation();
    FQuat TargetRotation = PathTransform.GetRotation();
    if (State.bHasInitialPlacement)
    {
        TargetLocation = PathTransform.TransformPosition(State.InitialPathLocalOffset);
        TargetRotation = PathTransform.GetRotation() * State.InitialPathRelativeRotation;
    }
    PartActor->SetActorLocationAndRotation(TargetLocation, TargetRotation, false, nullptr, ETeleportType::TeleportPhysics);
}

void ACMConveyorDistributorActor::UpdateMovementTimer()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const bool bShouldRun = HasAuthority() && !TrackedParts.IsEmpty() && MoveSpeed > UE_KINDA_SMALL_NUMBER;
    if (!bShouldRun)
    {
        StopMovementTimer();
        return;
    }
    if (World->GetTimerManager().IsTimerActive(MovementTimerHandle))
    {
        return;
    }

    LastMovementUpdateTime = World->GetTimeSeconds();
    World->GetTimerManager().SetTimer(MovementTimerHandle, this, &ThisClass::UpdateTrackedParts, FMath::Max(MovementUpdateInterval, 0.02f), true);
}

void ACMConveyorDistributorActor::StopMovementTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(MovementTimerHandle);
    }
    LastMovementUpdateTime = 0.0;
}

void ACMConveyorDistributorActor::UpdateTrackedParts()
{
    UWorld* World = GetWorld();
    if (!HasAuthority() || !World || TrackedParts.IsEmpty())
    {
        UpdateMovementTimer();
        return;
    }

    const double CurrentTime = World->GetTimeSeconds();
    const float DeltaSeconds = LastMovementUpdateTime > 0.0 ? static_cast<float>(CurrentTime - LastMovementUpdateTime) : 0.0f;
    LastMovementUpdateTime = CurrentTime;
    const float DirectionSign = Direction == ECMConveyorDistributorDirection::Forward ? 1.0f : -1.0f;
    const float SignedTravelDistance = DirectionSign * FMath::Max(MoveSpeed, 0.0f) * FMath::Max(DeltaSeconds, 0.0f);

    TArray<TWeakObjectPtr<ACMPartActorBase>> PartKeys;
    TrackedParts.GenerateKeyArray(PartKeys);
    TArray<TObjectPtr<ACMPartActorBase>> PartsToRemove;
    TArray<TObjectPtr<ACMPartActorBase>> PartsToHandoff;
    for (const TWeakObjectPtr<ACMPartActorBase>& PartKey : PartKeys)
    {
        ACMPartActorBase* PartActor = PartKey.Get();
        FTrackedPartState* State = TrackedParts.Find(PartKey);
        if (!IsValid(PartActor) || !State)
        {
            if (PartActor)
            {
                PartsToRemove.Add(PartActor);
            }
            else
            {
                TrackedParts.Remove(PartKey);
            }
            continue;
        }
        if (PartActor->IsAttached() || PartActor->GetAttachParentActor())
        {
            PartsToRemove.Add(PartActor);
            continue;
        }

        USplineComponent* CurrentPath = GetLanePath(State->Lane);
        const float PreviousProgress = State->Progress;
        bool bReachedEnd = false;
        State->Progress = CalculateNextProgress(State->Progress, CurrentPath ? CurrentPath->GetSplineLength() : 0.0f, SignedTravelDistance, bReachedEnd);
        if (ShouldCrossLane(PreviousProgress, State->Progress, RoutingMode))
        {
            State->Lane = State->Lane == ECMConveyorDistributorLane::Left ? ECMConveyorDistributorLane::Right : ECMConveyorDistributorLane::Left;
        }
        ApplyPartTransform(PartActor, *State);
        if (bReachedEnd)
        {
            PartsToHandoff.Add(PartActor);
        }
    }

    for (ACMPartActorBase* PartActor : PartsToRemove)
    {
        ReleasePart(PartActor, false);
    }
    for (ACMPartActorBase* PartActor : PartsToHandoff)
    {
        ReleasePart(PartActor, true);
    }
    UpdateMovementTimer();
}

void ACMConveyorDistributorActor::ReleasePart(ACMPartActorBase* PartActor, bool bTryConveyorHandoff)
{
    if (!IsValid(PartActor))
    {
        return;
    }

    if (bTryConveyorHandoff)
    {
        PartActor->Tags.Remove(CMConveyorTransportTags::Transporting);
        if (TryHandoffToConveyor(PartActor))
        {
            TrackedParts.Remove(PartActor);
            return;
        }

        PartActor->Tags.AddUnique(CMConveyorTransportTags::Transporting);
        return;
    }

    TrackedParts.Remove(PartActor);
    PartActor->Tags.Remove(CMConveyorTransportTags::Transporting);
}

bool ACMConveyorDistributorActor::TryHandoffToConveyor(ACMPartActorBase* PartActor) const
{
    UWorld* World = GetWorld();
    if (!World || !IsValid(PartActor))
    {
        return false;
    }

    for (TActorIterator<ACMConveyorSplineActor> It(World); It; ++It)
    {
        if (It->TryAcceptPart(PartActor))
        {
            return true;
        }
    }
    return false;
}

float ACMConveyorDistributorActor::CalculateNextProgress(float CurrentProgress, float PathLength, float SignedTravelDistance, bool& bOutReachedEnd)
{
    bOutReachedEnd = false;
    if (PathLength <= UE_KINDA_SMALL_NUMBER)
    {
        bOutReachedEnd = true;
        return FMath::Clamp(CurrentProgress, 0.0f, 1.0f);
    }

    const float NewProgress = CurrentProgress + SignedTravelDistance / PathLength;
    if (SignedTravelDistance > 0.0f && NewProgress >= 1.0f)
    {
        bOutReachedEnd = true;
        return 1.0f;
    }
    if (SignedTravelDistance < 0.0f && NewProgress <= 0.0f)
    {
        bOutReachedEnd = true;
        return 0.0f;
    }
    return FMath::Clamp(NewProgress, 0.0f, 1.0f);
}

bool ACMConveyorDistributorActor::ShouldCrossLane(float PreviousProgress, float NewProgress, ECMConveyorDistributorRoutingMode CurrentRoutingMode)
{
    if (CurrentRoutingMode != ECMConveyorDistributorRoutingMode::Cross)
    {
        return false;
    }
    return (PreviousProgress < 0.5f && NewProgress >= 0.5f)
        || (PreviousProgress > 0.5f && NewProgress <= 0.5f);
}
