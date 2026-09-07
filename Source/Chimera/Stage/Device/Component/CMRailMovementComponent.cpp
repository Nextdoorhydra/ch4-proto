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
}

bool UCMRailMovementComponent::IsConfigured() const
{
    return IsValid(Rail) && IsValid(MovingBody) && IsValid(Handle)
        && !Rail->IsClosedLoop() && Rail->GetNumberOfSplinePoints() >= 2
        && Rail->GetSplineLength() > UE_KINDA_SMALL_NUMBER
        && !MovingBody->IsSimulatingPhysics();
}

void UCMRailMovementComponent::ConfigureRail(USplineComponent* InRail, UBoxComponent* InMovingBody,
    UPrimitiveComponent* InHandle)
{
    StopMovement();
    Rail = InRail;
    MovingBody = InMovingBody;
    Handle = InHandle;
    if (GetOwner()->HasAuthority())
    {
        Progress = FMath::Clamp(InitialProgress, 0.0f, 1.0f);
        GetOwner()->ForceNetUpdate();
    }
    ApplyPose();
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

void UCMRailMovementComponent::ApplyPose()
{
    if (IsConfigured()) PreviewPose(Rail, MovingBody, Progress, bFollowRailRotation, RotationOffset);
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
    SetComponentTickEnabled(false);
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
    Progress = FMath::Clamp(InitialProgress, 0.0f, 1.0f);
    ApplyPose();
    PublishProgress(Previous);
}

void UCMRailMovementComponent::MoveToProgress(float TargetProgress)
{
    if (!GetOwner()->HasAuthority() || !IsConfigured() || !FMath::IsFinite(TargetProgress)) return;
    StopMovement();
    AutomaticTarget = FMath::Clamp(TargetProgress, 0.0f, 1.0f);
    if (FMath::IsNearlyEqual(Progress, AutomaticTarget))
    {
        if (Progress == 0.0f || Progress == 1.0f) OnEndpointReached.Broadcast(Progress == 1.0f);
        return;
    }
    bAutomaticMove = true;
    SetComponentTickEnabled(true);
}

bool UCMRailMovementComponent::IsSegmentBlocked(float FromDistance, float ToDistance) const
{
    if (!MovingBody->IsQueryCollisionEnabled()) return false;
    const FVector Start = Rail->GetLocationAtDistanceAlongSpline(FromDistance, ESplineCoordinateSpace::World);
    const FVector End = Rail->GetLocationAtDistanceAlongSpline(ToDistance, ESplineCoordinateSpace::World);
    const FQuat StartRotation = RailRotation(*Rail, FromDistance, bFollowRailRotation, RotationOffset);
    const FQuat EndRotation = RailRotation(*Rail, ToDistance, bFollowRailRotation, RotationOffset);
    const FVector Extent = MovingBody->GetScaledBoxExtent();
    // UE translation sweeps do not sweep rotation. Enclose the rotational arc conservatively.
    const float RotationPadding = 2.0f * Extent.Size()
        * FMath::Sin(StartRotation.AngularDistance(EndRotation) * 0.5f);
    const FCollisionShape Shape = FCollisionShape::MakeBox(Extent + FVector(RotationPadding));
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CMRailMove), false);
    Query.AddIgnoredComponent(MovingBody.Get());
    Query.AddIgnoredComponent(Handle.Get());
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
    FHitResult Hit;
    return GetWorld()->SweepSingleByChannel(Hit, Start, End, StartRotation,
        MovingBody->GetCollisionObjectType(), Shape, Query, Response)
        || GetWorld()->OverlapBlockingTestByChannel(End, EndRotation,
            MovingBody->GetCollisionObjectType(), FCollisionShape::MakeBox(Extent), Query, Response);
}

bool UCMRailMovementComponent::AdvanceDistance(float DeltaDistance)
{
    if (!GetOwner()->HasAuthority() || !IsConfigured() || !FMath::IsFinite(DeltaDistance)) return false;
    const float Length = Rail->GetSplineLength();
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
        if (IsSegmentBlocked(AcceptedDistance, Next))
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

void UCMRailMovementComponent::PublishProgress(float PreviousProgress)
{
    if (Progress == PreviousProgress) return;
    GetOwner()->ForceNetUpdate();
    OnProgressChanged.Broadcast(Progress);
    if (Progress == 0.0f || Progress == 1.0f) OnEndpointReached.Broadcast(Progress == 1.0f);
}

void UCMRailMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!GetOwner()->HasAuthority() || !IsConfigured())
    {
        StopMovement();
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
        const FVector LocalDelta = Rail->GetComponentTransform().InverseTransformVector(CurrentArmLocation - PreviousArmLocation);
        PreviousArmLocation = CurrentArmLocation; // Always consume blocked/perpendicular movement.
        const FVector Tangent = Rail->GetDirectionAtDistanceAlongSpline(Progress * Rail->GetSplineLength(), ESplineCoordinateSpace::Local);
        const float PullDistance = FVector::DotProduct(LocalDelta, Tangent) * FMath::Max(PullSensitivity, 0.01f);
        AdvanceDistance(FMath::Clamp(PullDistance, -MaxStep, MaxStep));
    }
    else if (bAutomaticMove)
    {
        AdvanceDistance(FMath::Clamp((AutomaticTarget - Progress) * Rail->GetSplineLength(), -MaxStep, MaxStep));
        if (FMath::IsNearlyEqual(Progress, AutomaticTarget)) bAutomaticMove = false;
    }
    SetComponentTickEnabled(bTrackingHold || bAutomaticMove);
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
