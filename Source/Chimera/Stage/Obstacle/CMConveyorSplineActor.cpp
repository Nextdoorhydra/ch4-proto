#include "Stage/Obstacle/CMConveyorSplineActor.h"

#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Stage/Obstacle/CMConveyorSegmentActor.h"
#include "TimerManager.h"

ACMConveyorSplineActor::ACMConveyorSplineActor()
{
    PrimaryActorTick.bCanEverTick = false;

    ConveyorPath = CreateDefaultSubobject<USplineComponent>(TEXT("ConveyorPath"));
    ConveyorPath->SetupAttachment(SceneRoot);
    ConveyorPath->SetAbsolute(false, false, false);
    ConveyorPath->ClearSplinePoints(false);
    ConveyorPath->AddSplinePoint(FVector::ZeroVector, ESplineCoordinateSpace::Local, false);
    ConveyorPath->AddSplinePoint(FVector(100.0f, 0.0f, 0.0f), ESplineCoordinateSpace::Local, false);
    ConveyorPath->SetSplinePointType(0, ESplinePointType::Linear, false);
    ConveyorPath->SetSplinePointType(1, ESplinePointType::Linear, false);
    ConveyorPath->SetClosedLoop(false);
    ConveyorPath->UpdateSpline();
    ConveyorPath->bEditableWhenInherited = true;
    ConveyorPath->SetVisibility(true);
    ConveyorPath->SetHiddenInGame(false);
    ConveyorPath->SetDrawDebug(true);
#if WITH_EDITORONLY_DATA
    ConveyorPath->EditorUnselectedSplineSegmentColor = FLinearColor(0.0f, 0.8f, 0.2f);
    ConveyorPath->EditorSelectedSplineSegmentColor = FLinearColor::Yellow;
    ConveyorPath->EditorTangentColor = FLinearColor(1.0f, 0.5f, 0.0f);
#endif
    ConveyorPath->SetMobility(EComponentMobility::Movable);

    PrimaryMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACMConveyorSplineActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ConveyorPath->SetMobility(EComponentMobility::Movable);
    ConveyorPath->SetAbsolute(false, false, false);
    ConveyorPath->AttachToComponent(SceneRoot, FAttachmentTransformRules::SnapToTargetIncludingScale);
    ConveyorPath->SetRelativeTransform(FTransform::Identity);
    ConveyorPath->SetVisibility(true);
    BuildSegmentChain();
    ConveyorPath->SetDrawDebug(RouteSegments.IsEmpty());
    ConveyorPath->SetClosedLoop(bClosedLoop, true);
    RebuildDetectionVolumes();
}

void ACMConveyorSplineActor::BeginPlay()
{
    Super::BeginPlay();
    BuildSegmentChain();
    RebuildDetectionVolumes();

    for (UBoxComponent* DetectionVolume : DetectionVolumes)
    {
        if (!DetectionVolume)
        {
            continue;
        }
        DetectionVolume->OnComponentBeginOverlap.RemoveAll(this);
        DetectionVolume->OnComponentEndOverlap.RemoveAll(this);
        DetectionVolume->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleDetectionBeginOverlap);
        DetectionVolume->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::HandleDetectionEndOverlap);
    }

    HandleObstacleActiveStateChanged(IsObstacleActive());
}

void ACMConveyorSplineActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopMovementTimer();
    for (UBoxComponent* DetectionVolume : DetectionVolumes)
    {
        if (DetectionVolume)
        {
            DetectionVolume->OnComponentBeginOverlap.RemoveAll(this);
            DetectionVolume->OnComponentEndOverlap.RemoveAll(this);
        }
    }
    TrackedParts.Empty();
    Super::EndPlay(EndPlayReason);
}

void ACMConveyorSplineActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ThisClass, Direction);
}

void ACMConveyorSplineActor::SetConveyorDirection(ECMConveyorDirection NewDirection)
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

void ACMConveyorSplineActor::ReverseConveyorDirection()
{
    if (!HasAuthority())
    {
        return;
    }
    SetConveyorDirection(Direction == ECMConveyorDirection::Forward ? ECMConveyorDirection::Reverse : ECMConveyorDirection::Forward);
}

void ACMConveyorSplineActor::RebuildRoute()
{
    BuildSegmentChain();
    ConveyorPath->SetDrawDebug(RouteSegments.IsEmpty());
    RebuildDetectionVolumes();
}

void ACMConveyorSplineActor::HandleObstacleActiveStateChanged(bool bIsActive)
{
    SetDetectionEnabled(bIsActive && HasAuthority());
    if (bIsActive && HasAuthority())
    {
        ScanInitialParts();
    }
    UpdateMovementTimer();
}

void ACMConveyorSplineActor::OnRep_Direction()
{
    OnDirectionChanged.Broadcast(Direction);
}

void ACMConveyorSplineActor::HandleDetectionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!HasAuthority() || !IsObstacleActive())
    {
        return;
    }
    RegisterPart(Cast<ACMPartActorBase>(OtherActor));
}

void ACMConveyorSplineActor::HandleDetectionEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
    if (!HasAuthority())
    {
        return;
    }

    const TWeakObjectPtr<ACMPartActorBase> PartKey(Cast<ACMPartActorBase>(OtherActor));
    if (FTrackedPartState* State = TrackedParts.Find(PartKey))
    {
        State->OverlapCount = FMath::Max(State->OverlapCount - 1, 0);
    }
}

void ACMConveyorSplineActor::RebuildDetectionVolumes()
{
    DestroyDetectionVolumes();
    TArray<USplineComponent*> ActivePaths;
    GetActivePaths(ActivePaths);
    const float SafeSegmentLength = FMath::Max(DetectionSegmentLength, 10.0f);
    for (USplineComponent* ActivePath : ActivePaths)
    {
        if (!ActivePath)
        {
            continue;
        }
        const float SplineLength = ActivePath->GetSplineLength();
        if (SplineLength <= UE_KINDA_SMALL_NUMBER)
        {
            continue;
        }
        const int32 SegmentCount = FMath::Max(FMath::CeilToInt(SplineLength / SafeSegmentLength), 1);
        DetectionVolumes.Reserve(DetectionVolumes.Num() + SegmentCount);
        for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
        {
            const float StartDistance = SplineLength * SegmentIndex / SegmentCount;
            const float EndDistance = SplineLength * (SegmentIndex + 1) / SegmentCount;
            const float MiddleDistance = (StartDistance + EndDistance) * 0.5f;
            const FTransform PathTransform = ActivePath->GetTransformAtDistanceAlongSpline(MiddleDistance, ESplineCoordinateSpace::World, true);
            const FVector VolumeLocation = PathTransform.GetLocation() + PathTransform.GetUnitAxis(EAxis::Z) * FMath::Max(DetectionHeight, 1.0f) * 0.5f;
            const FName VolumeName = MakeUniqueObjectName(this, UBoxComponent::StaticClass(), *FString::Printf(TEXT("ConveyorDetection_%d"), DetectionVolumes.Num()));
            UBoxComponent* DetectionVolume = NewObject<UBoxComponent>(this, VolumeName, RF_Transient | RF_Transactional);
            if (!DetectionVolume)
            {
                continue;
            }

            DetectionVolume->SetupAttachment(SceneRoot);
            DetectionVolume->SetMobility(EComponentMobility::Movable);
            DetectionVolume->SetCollisionObjectType(ECC_WorldDynamic);
            DetectionVolume->SetCollisionResponseToAllChannels(ECR_Overlap);
            DetectionVolume->SetGenerateOverlapEvents(true);
            DetectionVolume->SetCanEverAffectNavigation(false);
            DetectionVolume->SetHiddenInGame(true);
            DetectionVolume->ShapeColor = FColor::Green;
            DetectionVolume->SetBoxExtent(FVector((EndDistance - StartDistance) * 0.5f + DetectionOverlapPadding, FMath::Max(BeltWidth, 1.0f) * 0.5f, FMath::Max(DetectionHeight, 1.0f) * 0.5f));
            AddInstanceComponent(DetectionVolume);
            DetectionVolume->RegisterComponent();
            DetectionVolume->SetWorldLocationAndRotation(VolumeLocation, PathTransform.GetRotation());
            DetectionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            DetectionVolume->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleDetectionBeginOverlap);
            DetectionVolume->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::HandleDetectionEndOverlap);
            DetectionVolumes.Add(DetectionVolume);
        }
    }
}

void ACMConveyorSplineActor::BuildSegmentChain()
{
    RouteSegments.Empty();
    if (!IsValid(FirstSegment) || !GetWorld())
    {
        return;
    }

    TSet<TWeakObjectPtr<ACMConveyorSegmentActor>> VisitedSegments;
    ACMConveyorSegmentActor* CurrentSegment = FirstSegment;
    while (IsValid(CurrentSegment) && !VisitedSegments.Contains(CurrentSegment))
    {
        RouteSegments.Add(CurrentSegment);
        VisitedSegments.Add(CurrentSegment);
        const FTransform EndTransform = CurrentSegment->GetEndWorldTransform();
        ACMConveyorSegmentActor* BestCandidate = nullptr;
        float BestDistanceSquared = FMath::Square(FMath::Max(ConnectionTolerance, 0.1f));
        const float MinimumDirectionDot = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(ConnectionAngleTolerance, 0.0f, 90.0f)));
        for (TActorIterator<ACMConveyorSegmentActor> It(GetWorld()); It; ++It)
        {
            ACMConveyorSegmentActor* Candidate = *It;
            if (!IsValid(Candidate) || Candidate == CurrentSegment || (VisitedSegments.Contains(Candidate) && Candidate != FirstSegment))
            {
                continue;
            }

            const FTransform StartTransform = Candidate->GetStartWorldTransform();
            const float DistanceSquared = FVector::DistSquared(EndTransform.GetLocation(), StartTransform.GetLocation());
            const float DirectionDot = FVector::DotProduct(EndTransform.GetUnitAxis(EAxis::X), StartTransform.GetUnitAxis(EAxis::X));
            if (DistanceSquared <= BestDistanceSquared && DirectionDot >= MinimumDirectionDot)
            {
                BestDistanceSquared = DistanceSquared;
                BestCandidate = Candidate;
            }
        }
        if (BestCandidate == FirstSegment || !BestCandidate)
        {
            break;
        }
        CurrentSegment = BestCandidate;
    }
}

void ACMConveyorSplineActor::DestroyDetectionVolumes()
{
    for (UBoxComponent* DetectionVolume : DetectionVolumes)
    {
        if (!DetectionVolume)
        {
            continue;
        }
        RemoveInstanceComponent(DetectionVolume);
        DetectionVolume->DestroyComponent();
    }
    DetectionVolumes.Empty();
}

void ACMConveyorSplineActor::SetDetectionEnabled(bool bEnabled)
{
    for (UBoxComponent* DetectionVolume : DetectionVolumes)
    {
        if (DetectionVolume)
        {
            DetectionVolume->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
        }
    }
}

void ACMConveyorSplineActor::ScanInitialParts()
{
    if (!HasAuthority())
    {
        return;
    }

    TMap<TWeakObjectPtr<ACMPartActorBase>, int32> InitialOverlapCounts;
    for (UBoxComponent* DetectionVolume : DetectionVolumes)
    {
        if (!DetectionVolume)
        {
            continue;
        }

        TArray<AActor*> OverlappingActors;
        DetectionVolume->GetOverlappingActors(OverlappingActors, ACMPartActorBase::StaticClass());
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

void ACMConveyorSplineActor::RegisterPart(ACMPartActorBase* PartActor, int32 AddedOverlapCount)
{
    if (!HasAuthority() || !IsObstacleActive() || !IsValid(PartActor)
        || PartActor->IsAttached() || PartActor->GetAttachParentActor() || GetRouteLength() <= UE_KINDA_SMALL_NUMBER)
    {
        return;
    }

    const TWeakObjectPtr<ACMPartActorBase> PartKey(PartActor);
    if (FTrackedPartState* ExistingState = TrackedParts.Find(PartKey))
    {
        ExistingState->OverlapCount = FMath::Max(ExistingState->OverlapCount, AddedOverlapCount);
        return;
    }

    FTrackedPartState& State = TrackedParts.Add(PartKey);
    State.DistanceAlongRoute = FindClosestRouteDistance(PartActor->GetActorLocation());
    State.OverlapCount = FMath::Max(AddedOverlapCount, 1);
    if (bPreserveInitialPartPlacement)
    {
        float DistanceAlongPath = 0.0f;
        if (USplineComponent* ActivePath = GetPathAtRouteDistance(State.DistanceAlongRoute, DistanceAlongPath))
        {
            const FTransform PathTransform = ActivePath->GetTransformAtDistanceAlongSpline(DistanceAlongPath, ESplineCoordinateSpace::World, true);
            State.InitialPathLocalOffset = PathTransform.InverseTransformPosition(PartActor->GetActorLocation());
            State.InitialPathRelativeRotation = PathTransform.GetRotation().Inverse() * PartActor->GetActorQuat();
            State.InitialPathRelativeRotation.Normalize();
            State.bHasInitialPlacement = true;
        }
    }
    ApplyPartTransform(PartActor, State);
    UpdateMovementTimer();
}

bool ACMConveyorSplineActor::IsPartInsideDetection(const ACMPartActorBase* PartActor) const
{
    if (!IsValid(PartActor))
    {
        return false;
    }
    for (const UBoxComponent* DetectionVolume : DetectionVolumes)
    {
        if (DetectionVolume && DetectionVolume->IsOverlappingActor(PartActor))
        {
            return true;
        }
    }
    return false;
}

void ACMConveyorSplineActor::UpdateMovementTimer()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const bool bShouldRun = HasAuthority() && IsObstacleActive() && !TrackedParts.IsEmpty()
        && MoveSpeed > UE_KINDA_SMALL_NUMBER;
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

void ACMConveyorSplineActor::StopMovementTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(MovementTimerHandle);
    }
    LastMovementUpdateTime = 0.0;
}

void ACMConveyorSplineActor::UpdateTrackedParts()
{
    UWorld* World = GetWorld();
    if (!HasAuthority() || !IsObstacleActive() || !World
        || GetRouteLength() <= UE_KINDA_SMALL_NUMBER || TrackedParts.IsEmpty())
    {
        UpdateMovementTimer();
        return;
    }

    const double CurrentTime = World->GetTimeSeconds();
    const float DeltaSeconds = LastMovementUpdateTime > 0.0 ? static_cast<float>(CurrentTime - LastMovementUpdateTime) : 0.0f;
    LastMovementUpdateTime = CurrentTime;
    const float DirectionSign = Direction == ECMConveyorDirection::Forward ? 1.0f : -1.0f;
    const float SignedTravelDistance = DirectionSign * FMath::Max(MoveSpeed, 0.0f) * FMath::Max(DeltaSeconds, 0.0f);
    const float RouteLength = GetRouteLength();

    TArray<TWeakObjectPtr<ACMPartActorBase>> PartKeys;
    TrackedParts.GenerateKeyArray(PartKeys);
    TArray<TWeakObjectPtr<ACMPartActorBase>> PartsToRemove;
    TArray<TObjectPtr<ACMPartActorBase>> PartsToDestroy;
    for (const TWeakObjectPtr<ACMPartActorBase>& PartKey : PartKeys)
    {
        ACMPartActorBase* PartActor = PartKey.Get();
        FTrackedPartState* State = TrackedParts.Find(PartKey);
        if (!IsValid(PartActor) || !State)
        {
            PartsToRemove.Add(PartKey);
            continue;
        }
        if (PartActor->IsAttached() || PartActor->GetAttachParentActor())
        {
            PartsToRemove.Add(PartKey);
            continue;
        }
        if (State->OverlapCount <= 0 && !IsPartInsideDetection(PartActor))
        {
            PartsToRemove.Add(PartKey);
            continue;
        }

        bool bReachedEnd = false;
        State->DistanceAlongRoute = CalculateNextDistance(State->DistanceAlongRoute, RouteLength, SignedTravelDistance, bClosedLoop, bReachedEnd);
        if (bReachedEnd)
        {
            PartsToRemove.Add(PartKey);
            PartsToDestroy.Add(PartActor);
            continue;
        }
        ApplyPartTransform(PartActor, *State);
    }

    for (const TWeakObjectPtr<ACMPartActorBase>& PartKey : PartsToRemove)
    {
        TrackedParts.Remove(PartKey);
    }
    for (ACMPartActorBase* PartActor : PartsToDestroy)
    {
        if (IsValid(PartActor))
        {
            PartActor->Destroy();
        }
    }
    UpdateMovementTimer();
}

void ACMConveyorSplineActor::ApplyPartTransform(ACMPartActorBase* PartActor, const FTrackedPartState& State) const
{
    float DistanceAlongPath = 0.0f;
    USplineComponent* ActivePath = GetPathAtRouteDistance(State.DistanceAlongRoute, DistanceAlongPath);
    if (!IsValid(PartActor) || !ActivePath)
    {
        return;
    }

    const FTransform PathTransform = ActivePath->GetTransformAtDistanceAlongSpline(DistanceAlongPath, ESplineCoordinateSpace::World, true);
    FVector TargetLocation = PathTransform.GetLocation() + PathTransform.GetUnitAxis(EAxis::Z) * PartHeightOffset;
    FQuat TargetRotation = bAlignPartsToPath ? PathTransform.GetRotation() * PartRotationOffset.Quaternion() : PartActor->GetActorQuat();
    if (State.bHasInitialPlacement)
    {
        TargetLocation = PathTransform.TransformPosition(State.InitialPathLocalOffset);
        TargetRotation = PathTransform.GetRotation() * State.InitialPathRelativeRotation;
    }
    PartActor->SetActorLocationAndRotation(TargetLocation, TargetRotation, false, nullptr, ETeleportType::TeleportPhysics);
}

void ACMConveyorSplineActor::GetActivePaths(TArray<USplineComponent*>& OutPaths) const
{
    OutPaths.Reset();
    if (RouteSegments.IsEmpty())
    {
        if (ConveyorPath)
        {
            OutPaths.Add(ConveyorPath);
        }
        return;
    }
    for (const ACMConveyorSegmentActor* Segment : RouteSegments)
    {
        if (IsValid(Segment) && Segment->GetConveyorPath())
        {
            OutPaths.Add(Segment->GetConveyorPath());
        }
    }
}

float ACMConveyorSplineActor::GetRouteLength() const
{
    TArray<USplineComponent*> ActivePaths;
    GetActivePaths(ActivePaths);
    float RouteLength = 0.0f;
    for (const USplineComponent* ActivePath : ActivePaths)
    {
        RouteLength += ActivePath ? ActivePath->GetSplineLength() : 0.0f;
    }
    return RouteLength;
}

float ACMConveyorSplineActor::FindClosestRouteDistance(const FVector& WorldLocation) const
{
    TArray<USplineComponent*> ActivePaths;
    GetActivePaths(ActivePaths);
    float BestRouteDistance = 0.0f;
    float BestDistanceSquared = TNumericLimits<float>::Max();
    float PathStartDistance = 0.0f;
    for (USplineComponent* ActivePath : ActivePaths)
    {
        if (!ActivePath)
        {
            continue;
        }
        const float InputKey = ActivePath->FindInputKeyClosestToWorldLocation(WorldLocation);
        const float DistanceAlongPath = ActivePath->GetDistanceAlongSplineAtSplineInputKey(InputKey);
        const FVector PathLocation = ActivePath->GetLocationAtDistanceAlongSpline(DistanceAlongPath, ESplineCoordinateSpace::World);
        const float DistanceSquared = FVector::DistSquared(WorldLocation, PathLocation);
        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestRouteDistance = PathStartDistance + DistanceAlongPath;
        }
        PathStartDistance += ActivePath->GetSplineLength();
    }
    return BestRouteDistance;
}

USplineComponent* ACMConveyorSplineActor::GetPathAtRouteDistance(float DistanceAlongRoute, float& OutDistanceAlongPath) const
{
    TArray<USplineComponent*> ActivePaths;
    GetActivePaths(ActivePaths);
    float RemainingDistance = FMath::Max(DistanceAlongRoute, 0.0f);
    for (int32 PathIndex = 0; PathIndex < ActivePaths.Num(); ++PathIndex)
    {
        USplineComponent* ActivePath = ActivePaths[PathIndex];
        if (!ActivePath)
        {
            continue;
        }
        const float PathLength = ActivePath->GetSplineLength();
        if (RemainingDistance <= PathLength || PathIndex == ActivePaths.Num() - 1)
        {
            OutDistanceAlongPath = FMath::Clamp(RemainingDistance, 0.0f, PathLength);
            return ActivePath;
        }
        RemainingDistance -= PathLength;
    }
    OutDistanceAlongPath = 0.0f;
    return nullptr;
}

float ACMConveyorSplineActor::CalculateNextDistance(float CurrentDistance, float SplineLength, float SignedTravelDistance, bool bLoop, bool& bOutReachedEnd)
{
    bOutReachedEnd = false;
    if (SplineLength <= UE_KINDA_SMALL_NUMBER)
    {
        bOutReachedEnd = !bLoop;
        return 0.0f;
    }

    const float NextDistance = CurrentDistance + SignedTravelDistance;
    if (bLoop)
    {
        return FMath::Fmod(FMath::Fmod(NextDistance, SplineLength) + SplineLength, SplineLength);
    }
    if (SignedTravelDistance > 0.0f && NextDistance >= SplineLength)
    {
        bOutReachedEnd = true;
        return SplineLength;
    }
    if (SignedTravelDistance < 0.0f && NextDistance <= 0.0f)
    {
        bOutReachedEnd = true;
        return 0.0f;
    }
    return FMath::Clamp(NextDistance, 0.0f, SplineLength);
}
