#include "Stage/Device/Component/CMRailMovementComponent.h"

#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Parts/Arm/CMArmPart.h"
#include "Player/CMPartSlotComponent.h"

namespace
{
    FQuat RailRotation(const USplineComponent& Rail, float Distance, bool bFollow, const FRotator& Offset)
    {
        return (bFollow ? Rail.GetQuaternionAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World)
            : Rail.GetComponentQuat()) * Offset.Quaternion();
    }
}

UCMRailMovementComponent::UCMRailMovementComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    SetIsReplicatedByDefault(true);
}

void UCMRailMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ThisClass, Progress);
    DOREPLIFETIME(ThisClass, ActiveSegmentIndex);
}

bool UCMRailMovementComponent::IsConfigured() const
{
    const USplineComponent* ActiveRail = GetActiveRail();
    return IsValid(ActiveRail) && IsValid(MovingBody) && IsValid(Handle)
        && !ActiveRail->IsClosedLoop() && ActiveRail->GetNumberOfSplinePoints() >= 2
        && ActiveRail->GetSplineLength() > UE_KINDA_SMALL_NUMBER
        && !MovingBody->IsSimulatingPhysics();
}

USplineComponent* UCMRailMovementComponent::GetActiveRail() const
{
    return RailSegments.IsValidIndex(ActiveSegmentIndex)
        ? RailSegments[ActiveSegmentIndex].Get()
        : nullptr;
}

bool UCMRailMovementComponent::IsEndpointConnected(
    int32 SegmentIndex,
    bool bAtEnd) const
{
    if (!RailSegments.IsValidIndex(SegmentIndex))
    {
        return false;
    }

    const USplineComponent* Segment = RailSegments[SegmentIndex];
    const float SegmentDistance = bAtEnd ? Segment->GetSplineLength() : 0.0f;
    const FVector Endpoint = Segment->GetLocationAtDistanceAlongSpline(
        SegmentDistance,
        ESplineCoordinateSpace::World);

    for (int32 OtherIndex = 0; OtherIndex < RailSegments.Num(); ++OtherIndex)
    {
        if (OtherIndex == SegmentIndex || !IsValid(RailSegments[OtherIndex]))
        {
            continue;
        }

        const USplineComponent* Other = RailSegments[OtherIndex];
        const FVector OtherStart = Other->GetLocationAtDistanceAlongSpline(
            0.0f,
            ESplineCoordinateSpace::World);
        const FVector OtherEnd = Other->GetLocationAtDistanceAlongSpline(
            Other->GetSplineLength(),
            ESplineCoordinateSpace::World);
        const float Tolerance = FMath::Max(RailConnectionTolerance, 0.0f);
        if (Endpoint.Equals(OtherStart, Tolerance)
            || Endpoint.Equals(OtherEnd, Tolerance))
        {
            return true;
        }
    }

    return false;
}

bool UCMRailMovementComponent::IsAtConnectedJunction() const
{
    return (FMath::IsNearlyZero(Progress)
            && IsEndpointConnected(ActiveSegmentIndex, false))
        || (FMath::IsNearlyEqual(Progress, 1.0f)
            && IsEndpointConnected(ActiveSegmentIndex, true));
}

void UCMRailMovementComponent::ConfigureRail(USplineComponent* InRail, UBoxComponent* InMovingBody,
    UPrimitiveComponent* InHandle, USceneComponent* InVisual)
{
    ConfigureRailGraph({InRail}, InMovingBody, InHandle, InVisual);
}

void UCMRailMovementComponent::ConfigureBranchedRail(
    USplineComponent* InEntryRail,
    const TArray<USplineComponent*>& InBranchRails,
    UBoxComponent* InMovingBody,
    UPrimitiveComponent* InHandle,
    USceneComponent* InVisual)
{
    TArray<USplineComponent*> Segments;
    Segments.Reserve(InBranchRails.Num() + 1);
    Segments.Add(InEntryRail);
    for (USplineComponent* BranchRail : InBranchRails)
    {
        Segments.Add(BranchRail);
    }
    ConfigureRailGraph(Segments, InMovingBody, InHandle, InVisual);
}

void UCMRailMovementComponent::ConfigureRailGraph(
    const TArray<USplineComponent*>& InRailSegments,
    UBoxComponent* InMovingBody,
    UPrimitiveComponent* InHandle,
    USceneComponent* InVisual)
{
    StopMovement();
    RailSegments.Reset(InRailSegments.Num());
    for (USplineComponent* Segment : InRailSegments)
    {
        if (IsValid(Segment) && !RailSegments.Contains(Segment))
        {
            RailSegments.Add(Segment);
        }
    }
    MovingBody = InMovingBody;
    Handle = InHandle;
    Visual = InVisual;
    if (Visual)
    {
        VisualTargetRelativeTransform = Visual->GetRelativeTransform();
    }
    if (GetOwner()->HasAuthority())
    {
        ActiveSegmentIndex = FMath::Clamp(
            InitialSegmentIndex,
            0,
            FMath::Max(RailSegments.Num() - 1, 0));
        Progress = FMath::Clamp(InitialProgress, 0.0f, 1.0f);
        GetOwner()->ForceNetUpdate();
    }
    ApplyPose(true);
}

void UCMRailMovementComponent::PreviewPose(USplineComponent* InRail, UBoxComponent* Body,
    float AtProgress, bool bFollowRotation, const FRotator& Offset)
{
    if (!IsValid(InRail) || !IsValid(Body)) return;
    const float Distance = FMath::Clamp(AtProgress, 0.0f, 1.0f) * InRail->GetSplineLength();
    Body->SetWorldLocationAndRotation(
        InRail->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World),
        RailRotation(*InRail, Distance, bFollowRotation, Offset), false, nullptr, ETeleportType::TeleportPhysics);
}

void UCMRailMovementComponent::ApplyPose(bool bInstant)
{
    if (USplineComponent* ActiveRail = GetActiveRail(); IsConfigured() && ActiveRail)
    {
        const FTransform PreviousVisualWorldTransform = Visual
            ? Visual->GetComponentTransform()
            : FTransform::Identity;
        PreviewPose(ActiveRail, MovingBody, Progress, bFollowRailRotation, RotationOffset);
        if (Visual)
        {
            if (bInstant)
            {
                Visual->SetRelativeTransform(VisualTargetRelativeTransform);
                bVisualSmoothing = false;
            }
            else
            {
                Visual->SetWorldTransform(
                    PreviousVisualWorldTransform,
                    false,
                    nullptr,
                    ETeleportType::TeleportPhysics);
                bVisualSmoothing = true;
                SetComponentTickEnabled(true);
            }
        }
    }
}

void UCMRailMovementComponent::SmoothVisual(float DeltaTime)
{
    if (!bVisualSmoothing || !Visual)
    {
        bVisualSmoothing = false;
        return;
    }

    const float Alpha = 1.0f - FMath::Exp(
        -FMath::Max(MovementSmoothingSpeed, 0.1f) * FMath::Max(DeltaTime, 0.0f));
    const FTransform Current = Visual->GetRelativeTransform();
    FTransform Smoothed;
    Smoothed.Blend(Current, VisualTargetRelativeTransform, Alpha);
    Visual->SetRelativeTransform(Smoothed);

    if (Smoothed.GetLocation().Equals(VisualTargetRelativeTransform.GetLocation(), 0.1f)
        && Smoothed.GetRotation().AngularDistance(
            VisualTargetRelativeTransform.GetRotation()) <= FMath::DegreesToRadians(0.1f))
    {
        Visual->SetRelativeTransform(VisualTargetRelativeTransform);
        bVisualSmoothing = false;
    }
}

bool UCMRailMovementComponent::QueryArmHold(ACMArmPart* Arm, FCMArmHoldSpec& OutSpec) const
{
    if (!CanArmHold(Arm)) return false;
    // The owning actor may be hit anywhere, but only its handle is a valid grip.
    const FVector Grip = Handle->GetComponentLocation();
    OutSpec.Priority = 100;
    OutSpec.HoldLocation = Grip;
    OutSpec.HoldNormal = Handle->GetForwardVector();
    OutSpec.TargetComponent = Handle;
    OutSpec.bUsePhysicsHandle = false;
    return true;
}

bool UCMRailMovementComponent::CanArmHold(ACMArmPart* Arm) const
{
    if (!GetOwner()->HasAuthority() || !IsConfigured() || !bInteractionEnabled
        || bTrackingHold || bAutomaticMove || !IsValid(Arm) || !Arm->IsOperational()) return false;

    return FVector::Dist(Arm->GetActorLocation(), Handle->GetComponentLocation())
        <= Arm->GetHoldRange() + Arm->GetHoldRadius();
}

bool UCMRailMovementComponent::BeginArmHold(ACMArmPart* Arm)
{
    FCMArmHoldSpec Spec;
    if (!QueryArmHold(Arm, Spec)) return false;
    HoldingArm = Arm;
    PreviousArmLocation = Arm->GetActorLocation();
    bTrackingHold = true;
    SetComponentTickEnabled(true);
    return true;
}

void UCMRailMovementComponent::EndArmHold(ACMArmPart* Arm)
{
    if (GetOwner()->HasAuthority() && HoldingArm.Get() == Arm) StopMovement();
}

void UCMRailMovementComponent::ReleaseArm()
{
    ACMArmPart* Arm = HoldingArm.Get();
    HoldingArm.Reset();
    bTrackingHold = false;
    if (IsValid(Arm)) Arm->EndGroundAnchor();
}

void UCMRailMovementComponent::StopMovement()
{
    if (!GetOwner()->HasAuthority()) return;
    ReleaseArm();
    bAutomaticMove = false;
    SetComponentTickEnabled(bVisualSmoothing);
}

void UCMRailMovementComponent::SetInteractionEnabled(bool bEnabled)
{
    if (!GetOwner()->HasAuthority()) return;
    bInteractionEnabled = bEnabled;
    if (!bEnabled) StopMovement();
}

void UCMRailMovementComponent::ResetRail()
{
    if (!GetOwner()->HasAuthority()) return;
    StopMovement();
    const float Previous = Progress;
    ActiveSegmentIndex = FMath::Clamp(
        InitialSegmentIndex,
        0,
        FMath::Max(RailSegments.Num() - 1, 0));
    Progress = FMath::Clamp(InitialProgress, 0.0f, 1.0f);
    ApplyPose(true);
    PublishProgress(Previous);
}

void UCMRailMovementComponent::MoveToProgress(float TargetProgress)
{
    if (!GetOwner()->HasAuthority() || !IsConfigured() || !FMath::IsFinite(TargetProgress)) return;
    StopMovement();
    AutomaticTarget = FMath::Clamp(TargetProgress, 0.0f, 1.0f);
    if (FMath::IsNearlyEqual(Progress, AutomaticTarget))
    {
        const bool bAtOuterStart = Progress == 0.0f
            && !IsEndpointConnected(ActiveSegmentIndex, false);
        const bool bAtOuterEnd = Progress == 1.0f
            && !IsEndpointConnected(ActiveSegmentIndex, true);
        if (bAtOuterStart || bAtOuterEnd)
        {
            OnEndpointReached.Broadcast(bAtOuterEnd);
        }
        return;
    }
    bAutomaticMove = true;
    SetComponentTickEnabled(true);
}

bool UCMRailMovementComponent::IsSegmentBlocked(
    float FromDistance,
    float ToDistance,
    const AActor* IgnoredActor) const
{
    if (!MovingBody->IsQueryCollisionEnabled()) return false;
    const USplineComponent* ActiveRail = GetActiveRail();
    if (!ActiveRail) return true;
    const FVector Start = ActiveRail->GetLocationAtDistanceAlongSpline(FromDistance, ESplineCoordinateSpace::World);
    const FVector End = ActiveRail->GetLocationAtDistanceAlongSpline(ToDistance, ESplineCoordinateSpace::World);
    const FQuat StartRotation = RailRotation(*ActiveRail, FromDistance, bFollowRailRotation, RotationOffset);
    const FQuat EndRotation = RailRotation(*ActiveRail, ToDistance, bFollowRailRotation, RotationOffset);
    const FVector Extent = MovingBody->GetScaledBoxExtent();
    // UE translation sweeps do not sweep rotation. Enclose the rotational arc conservatively.
    const float RotationPadding = 2.0f * Extent.Size()
        * FMath::Sin(StartRotation.AngularDistance(EndRotation) * 0.5f);
    const FCollisionShape Shape = FCollisionShape::MakeBox(Extent + FVector(RotationPadding));
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CMRailMove), false);
    Query.AddIgnoredComponent(MovingBody.Get());
    Query.AddIgnoredComponent(Handle.Get());
    if (IgnoredActor)
    {
        Query.AddIgnoredActor(IgnoredActor);
    }
    if (ACMArmPart* Arm = HoldingArm.Get())
    {
        Query.AddIgnoredActor(Arm);
        if (const UCMPartSlotComponent* PartSlot = Arm->GetAttachedPartSlot())
        {
            Query.AddIgnoredActor(PartSlot->GetOwner());
        }
    }
    // Do NOT ignore the owner: a separate frame component must still block its moving panel.
    const FCollisionResponseParams Response(MovingBody->GetCollisionResponseToChannels());
    TArray<FHitResult> Hits;
    if (!GetWorld()->SweepMultiByChannel(Hits, Start, End, StartRotation,
        MovingBody->GetCollisionObjectType(), Shape, Query, Response))
    {
        return false;
    }

    const FVector MoveDirection = (End - Start).GetSafeNormal();
    for (const FHitResult& Hit : Hits)
    {
        if (!Hit.bBlockingHit)
        {
            continue;
        }

        // A rail body may rest slightly inside the floor. Only surfaces facing
        // against this step block it; floor/ceiling contacts parallel to travel do not.
        if (Hit.Normal.IsNearlyZero()
            || FVector::DotProduct(MoveDirection, Hit.Normal) < -UE_KINDA_SMALL_NUMBER)
        {
            return true;
        }
    }

    return false;
}

bool UCMRailMovementComponent::AdvanceDistance(float DeltaDistance, const AActor* IgnoredActor)
{
    if (!GetOwner()->HasAuthority() || !IsConfigured() || !FMath::IsFinite(DeltaDistance)) return false;
    const USplineComponent* ActiveRail = GetActiveRail();
    if (!ActiveRail) return false;
    const float Length = ActiveRail->GetSplineLength();
    const float Previous = Progress;
    const float StartDistance = Progress * Length;
    const float Target = FMath::Clamp(StartDistance + DeltaDistance, 0.0f, Length);
    if (FMath::IsNearlyEqual(StartDistance, Target)) return false;
    // Bound query work on a hitch; discard excess pull rather than storing a future jump.
    const float StepSize = FMath::Clamp(CollisionStepDistance, 0.1f, 10.0f);
    const float LimitedTarget = StartDistance + FMath::Clamp(Target - StartDistance, -StepSize * 128, StepSize * 128);
    const int32 Steps = FMath::Clamp(FMath::CeilToInt(FMath::Abs(LimitedTarget - StartDistance) / StepSize), 1, 128);
    float AcceptedDistance = StartDistance;
    bool bBlocked = false;
    for (int32 Index = 1; Index <= Steps; ++Index)
    {
        const float Next = FMath::Lerp(StartDistance, LimitedTarget, float(Index) / Steps);
        if (IsSegmentBlocked(AcceptedDistance, Next, IgnoredActor))
        {
            bBlocked = true;
            break;
        }
        AcceptedDistance = Next;
    }
    Progress = FMath::Clamp(AcceptedDistance / Length, 0.0f, 1.0f);
    ApplyPose();
    if (bBlocked) bAutomaticMove = false;
    PublishProgress(Previous);
    if (bBlocked) OnMovementBlocked.Broadcast();
    return Progress != Previous;
}

bool UCMRailMovementComponent::SelectRailAtEndpoint(FVector WorldDirection)
{
    if (!GetOwner()->HasAuthority()
        || (!FMath::IsNearlyZero(Progress) && !FMath::IsNearlyEqual(Progress, 1.0f))
        || !WorldDirection.Normalize())
    {
        return false;
    }

    const USplineComponent* CurrentSegment = GetActiveRail();
    if (!CurrentSegment)
    {
        return false;
    }

    const bool bCurrentAtEnd = FMath::IsNearlyEqual(Progress, 1.0f);
    const float CurrentDistance = bCurrentAtEnd
        ? CurrentSegment->GetSplineLength()
        : 0.0f;
    const FVector Endpoint = CurrentSegment->GetLocationAtDistanceAlongSpline(
        CurrentDistance,
        ESplineCoordinateSpace::World);
    const FVector CurrentExitDirection = (bCurrentAtEnd ? -1.0f : 1.0f)
        * CurrentSegment->GetDirectionAtDistanceAlongSpline(
            CurrentDistance,
            ESplineCoordinateSpace::World);

    float BestDot = FVector::DotProduct(WorldDirection, CurrentExitDirection);
    int32 BestSegmentIndex = ActiveSegmentIndex;
    bool bBestAtEnd = bCurrentAtEnd;
    const float Tolerance = FMath::Max(RailConnectionTolerance, 0.0f);

    for (int32 SegmentIndex = 0; SegmentIndex < RailSegments.Num(); ++SegmentIndex)
    {
        const USplineComponent* Candidate = RailSegments[SegmentIndex];
        if (SegmentIndex == ActiveSegmentIndex || !IsValid(Candidate)
            || Candidate->IsClosedLoop()
            || Candidate->GetNumberOfSplinePoints() < 2
            || Candidate->GetSplineLength() <= UE_KINDA_SMALL_NUMBER)
        {
            continue;
        }

        for (const bool bCandidateAtEnd : {false, true})
        {
            const float CandidateDistance = bCandidateAtEnd
                ? Candidate->GetSplineLength()
                : 0.0f;
            const FVector CandidateEndpoint = Candidate->GetLocationAtDistanceAlongSpline(
                CandidateDistance,
                ESplineCoordinateSpace::World);
            if (!Endpoint.Equals(CandidateEndpoint, Tolerance))
            {
                continue;
            }

            const FVector ExitDirection = (bCandidateAtEnd ? -1.0f : 1.0f)
                * Candidate->GetDirectionAtDistanceAlongSpline(
                    CandidateDistance,
                    ESplineCoordinateSpace::World);
            const float DirectionDot = FVector::DotProduct(
                WorldDirection,
                ExitDirection);
            if (DirectionDot > BestDot)
            {
                BestDot = DirectionDot;
                BestSegmentIndex = SegmentIndex;
                bBestAtEnd = bCandidateAtEnd;
            }
        }
    }

    if (BestDot < FMath::Clamp(BranchSelectionDotThreshold, -1.0f, 1.0f))
    {
        return false;
    }

    ActiveSegmentIndex = BestSegmentIndex;
    Progress = bBestAtEnd ? 1.0f : 0.0f;
    ApplyPose();
    GetOwner()->ForceNetUpdate();
    return true;
}

bool UCMRailMovementComponent::AdvanceFromWorldDelta(
    FVector WorldDelta,
    const AActor* IgnoredActor)
{
    if (!GetOwner()->HasAuthority() || !IsConfigured() || WorldDelta.IsNearlyZero()
        || WorldDelta.ContainsNaN())
    {
        return false;
    }

    if ((FMath::IsNearlyZero(Progress) || FMath::IsNearlyEqual(Progress, 1.0f))
        && !SelectRailAtEndpoint(WorldDelta))
    {
        return false;
    }

    const USplineComponent* ActiveRail = GetActiveRail();
    if (!ActiveRail)
    {
        return false;
    }

    const FVector Tangent = ActiveRail->GetDirectionAtDistanceAlongSpline(
        Progress * ActiveRail->GetSplineLength(),
        ESplineCoordinateSpace::World);
    return AdvanceDistance(
        FVector::DotProduct(WorldDelta, Tangent),
        IgnoredActor);
}

bool UCMRailMovementComponent::TryCollisionPush(
    AActor* PushingActor,
    FVector WorldPushDirection,
    float DeltaTime)
{
    if (!bAllowCollisionPush || !bInteractionEnabled || bTrackingHold
        || bAutomaticMove || !IsValid(PushingActor) || !GetOwner()->HasAuthority()
        || !IsConfigured() || DeltaTime <= 0.0f || !FMath::IsFinite(DeltaTime)
        || WorldPushDirection.ContainsNaN())
    {
        return false;
    }

    // 미는 힘과 수직 높이는 사용하지 않는다. 접촉한 주체에서 레일 바디를
    // 향하는 평면 방향만 사용해 프레임마다 일정한 거리만 진행한다.
    WorldPushDirection.Z = 0.0f;
    if (!WorldPushDirection.Normalize())
    {
        return false;
    }

    const float PushSpeed = FMath::Min(
        FMath::Max(CollisionPushSpeed, 0.01f),
        FMath::Max(MaxMoveSpeed, 1.0f));
    return AdvanceFromWorldDelta(
        WorldPushDirection * PushSpeed * DeltaTime,
        PushingActor);
}

void UCMRailMovementComponent::PublishProgress(float PreviousProgress)
{
    if (Progress == PreviousProgress) return;
    GetOwner()->ForceNetUpdate();
    OnProgressChanged.Broadcast(Progress);
    const bool bAtOuterStart = Progress == 0.0f
        && !IsEndpointConnected(ActiveSegmentIndex, false);
    const bool bAtOuterEnd = Progress == 1.0f
        && !IsEndpointConnected(ActiveSegmentIndex, true);
    if (bAtOuterStart || bAtOuterEnd)
    {
        OnEndpointReached.Broadcast(bAtOuterEnd);
    }
}

void UCMRailMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!IsConfigured())
    {
        SetComponentTickEnabled(false);
        return;
    }
    if (!GetOwner()->HasAuthority())
    {
        SmoothVisual(DeltaTime);
        SetComponentTickEnabled(bVisualSmoothing);
        return;
    }
    const float MaxStep = FMath::Max(MaxMoveSpeed, 1.0f) * FMath::Max(DeltaTime, 0.0f);
    if (bTrackingHold)
    {
        ACMArmPart* Arm = HoldingArm.Get();
        if (!IsValid(Arm) || !Arm->IsOperational() || !Arm->IsHolding()
            || FVector::Dist(Arm->GetActorLocation(), Handle->GetComponentLocation()) > ReleaseDistance)
        {
            StopMovement();
            return;
        }
        const FVector CurrentArmLocation = Arm->GetActorLocation();
        const FVector WorldDelta = CurrentArmLocation - PreviousArmLocation;
        PreviousArmLocation = CurrentArmLocation; // Always consume blocked/perpendicular movement.
        AdvanceFromWorldDelta(
            (WorldDelta * FMath::Max(PullSensitivity, 0.01f)).GetClampedToMaxSize(MaxStep));
    }
    else if (bAutomaticMove)
    {
        const USplineComponent* ActiveRail = GetActiveRail();
        AdvanceDistance(FMath::Clamp(
            (AutomaticTarget - Progress) * ActiveRail->GetSplineLength(),
            -MaxStep,
            MaxStep));
        if (FMath::IsNearlyEqual(Progress, AutomaticTarget)) bAutomaticMove = false;
    }
    SmoothVisual(DeltaTime);
    SetComponentTickEnabled(bTrackingHold || bAutomaticMove || bVisualSmoothing);
}

void UCMRailMovementComponent::OnRep_Progress()
{
    ApplyPose();
    OnProgressChanged.Broadcast(Progress);
}

void UCMRailMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopMovement();
    Super::EndPlay(EndPlayReason);
}
