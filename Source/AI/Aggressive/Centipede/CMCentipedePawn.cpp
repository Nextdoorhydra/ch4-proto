#include "Aggressive/Centipede/CMCentipedePawn.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Aggressive/Common/Animation/CMAIProceduralLegComponent.h"
#include "Aggressive/Common/Behavior/CMAggressiveBehaviorComponent.h"
#include "Aggressive/Common/Movement/CMAggressiveMovementCommandComponent.h"
#include "Aggressive/Common/Movement/CMAggressiveOmnidirectionalPathComponent.h"
#include "Aggressive/Common/Movement/CMAIFixedLegActuatorComponent.h"
#include "Aggressive/Common/Movement/CMGroundPlacementBoxComponent.h"
#include "Aggressive/Common/Perception/CMAggressiveSightComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace CMCentipedeBody
{
    constexpr int32 SegmentCount = 4;
    constexpr int32 JointCount = SegmentCount - 1;
    constexpr float GroundContactHeight = -150.0f;
    constexpr float LegHalfHeight = 20.0f;
    constexpr float LegLateralOffset = 90.0f;
    constexpr float TrailSampleDistance = 20.0f;
} // namespace CMCentipedeBody

// 네 개의 관절 몸통과 여덟 다리 및 양방향 시야·학습 이동 컴포넌트를 구성한다.
ACMCentipedePawn::ACMCentipedePawn()
{
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
    bReplicates = true;
    SetReplicateMovement(false);
    ConfiguredKnockbackDistanceCm = 0.0f;

    Behavior = CreateDefaultSubobject<UCMAggressiveBehaviorComponent>(TEXT("Behavior"));
    Behavior->ConfigureProfile(ECMAggressiveBehaviorProfile::Centipede);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));

    UCMGroundPlacementBoxComponent* GroundedHeadBody = CreateDefaultSubobject<UCMGroundPlacementBoxComponent>(TEXT("Segment00Body"));
    HeadBody = GroundedHeadBody;
    SetRootComponent(HeadBody);
    HeadBody->SetBoxExtent(FVector(BodyLength, BodyWidth, BodyHeight) * 0.5f);
    GroundedHeadBody->SetGroundContactHeight(CMCentipedeBody::GroundContactHeight);
    HeadBody->SetCollisionProfileName(TEXT("PhysicsActor"));
    HeadBody->SetSimulatePhysics(true);
    HeadBody->SetIsReplicated(true);
    HeadBody->SetCanEverAffectNavigation(false);
    BodySegments.Add(HeadBody);

    LegActuator = CreateDefaultSubobject<UCMAIFixedLegActuatorComponent>(TEXT("FixedLegActuator"));
    MovementCommand = CreateDefaultSubobject<UCMAggressiveMovementCommandComponent>(TEXT("MovementCommand"));
    PathMovement = CreateDefaultSubobject<UCMAggressiveOmnidirectionalPathComponent>(TEXT("PathMovement"));
    PathMovement->SetPolicyControlEnabled(true);
    PathMovement->SetNavigationAgentName(TEXT("CentipedeAI"));
    PathMovement->SetIntermediatePathPointTolerances(80.0f, 130.0f);
    PathMovement->SetMaximumPathSegmentLength(250.0f);
    PathMovement->SetIntermediatePathPointJitterRadius(50.0f);
    PathMovement->SetIntermediatePathPointWallClearance(250.0f);
    PathMovement->SetMinimumPathPointSpacing(0.0f);
    PathMovement->SetRebuildPathWhenIntermediatePointPassed(true);

    LegActuationSettings.ImpulseMagnitude = 9000.0f;
    LegActuationSettings.CooldownSeconds = 0.08f;
    LegActuationSettings.GroundCheckRadius = 14.0f;
    LegActuationSettings.GroundContactDistance = 15.0f;

    AddBodySegment(0, CubeMeshAsset.Object);
    for (int32 SegmentIndex = 1; SegmentIndex < CMCentipedeBody::SegmentCount; ++SegmentIndex)
        AddBodySegment(SegmentIndex, CubeMeshAsset.Object);
    for (int32 SegmentIndex = 0; SegmentIndex < CMCentipedeBody::SegmentCount; ++SegmentIndex)
    {
        AddLeg(SegmentIndex, true, CubeMeshAsset.Object);
        AddLeg(SegmentIndex, false, CubeMeshAsset.Object);
    }

    HeadSight = CreateDefaultSubobject<UCMAggressiveSightComponent>(TEXT("HeadSight"));
    HeadSight->SetupAttachment(HeadBody);
    HeadSight->SetSightDefaults(1000.0f, 60.0f, 180.0f);

    TailSight = CreateDefaultSubobject<UCMAggressiveSightComponent>(TEXT("TailSight"));
    TailSight->SetupAttachment(BodySegments.Last());
    TailSight->SetSightDefaults(1000.0f, 60.0f, 180.0f);
    TailSight->SetSightForwardReversed(true);

    TargetJointAnglesDegrees.Init(0.0f, CMCentipedeBody::JointCount);
}

// 몸통 물리와 관절 제약 및 다리 구동 상태를 실제 월드 기준으로 초기화한다.
void ACMCentipedePawn::BeginPlay()
{
    Super::BeginPlay();
    LegActuator->ResolveInitialGroundPenetration(HeadBody, LegContactPoints, LegActuationSettings);
    ApplyBodySettings();
    CreateSegmentConstraints();
    LegActuator->InitializeLegs(LegContactPoints.Num());
    ResetHeadTrail();
}

void ACMCentipedePawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    DestroySegmentConstraints();
    Super::EndPlay(EndPlayReason);
}

// 진행 선두 방향과 정렬 상태를 반영해 지정한 다리에 이동 임펄스를 적용한다.
bool ACMCentipedePawn::ActivateLeg(int32 LegIndex)
{
    if (!HasAuthority() || !LegActuator || !LegBodies.IsValidIndex(LegIndex) || !LegContactPoints.IsValidIndex(LegIndex))
        return false;

    FCMAIFixedLegActuationResult Result;
    const FVector LocalImpulseDirection = bTailLeading ? FVector::BackwardVector : FVector::ForwardVector;
    FCMAIFixedLegActuationSettings EffectiveSettings = LegActuationSettings;
    const float MovementOutputScale = bAligningLeadingEnd ? FMath::Clamp(AlignmentMovementOutputScale, 0.0f, 1.0f) : 1.0f;
    EffectiveSettings.ImpulseMagnitude *= MovementOutputScale;
    const bool bActivated = LegActuator->TryActivateLeg(LegIndex, LegBodies[LegIndex], LegContactPoints[LegIndex], LocalImpulseDirection, EffectiveSettings, Result);
    if (bActivated)
        LegActuator->LimitPlanarSpeed(LegBodies[LegIndex], MaxPlanarSpeed * MovementOutputScale);

    return bActivated;
}

// 정책이 선택한 여러 다리를 구동하고 모든 몸통 세그먼트의 평면 속도를 제한한다.
int32 ACMCentipedePawn::ActivateLegs(const TArray<int32>& LegIndices)
{
    if (!HasAuthority() || !LegActuator)
        return 0;

    int32 ActivatedCount = 0;
    for (const int32 LegIndex : LegIndices)
        ActivatedCount += ActivateLeg(LegIndex) ? 1 : 0;
    const float MovementOutputScale = bAligningLeadingEnd ? FMath::Clamp(AlignmentMovementOutputScale, 0.0f, 1.0f) : 1.0f;
    for (UBoxComponent* Segment : BodySegments)
        LegActuator->LimitPlanarSpeed(Segment, MaxPlanarSpeed * MovementOutputScale);

    return ActivatedCount;
}

// 학습용 곡률을 해제하고 유리한 말단을 선두로 선택한 뒤 경로 이동을 시작한다.
bool ACMCentipedePawn::StartPathMoveToLocation(FVector WorldGoal, float AcceptanceRadius)
{
    if (!PathMovement || !MovementCommand)
        return false;
    TrainingCurveProfile = 0;
    bAligningLeadingEnd = false;
    ResetHeadTrail();
    PathMovement->SetPolicyControlEnabled(true);

    return PathMovement->StartPathMove(WorldGoal, AcceptanceRadius);
}

void ACMCentipedePawn::StopPathMove()
{
    bAligningLeadingEnd = false;
    if (PathMovement)
        PathMovement->StopPathMove();
    if (MovementCommand)
        MovementCommand->ClearMovementGoal();
}

void ACMCentipedePawn::StopAggressiveMovementForReaction()
{
    StopPathMove();
}

int32 ACMCentipedePawn::GetLegCount() const
{
    return LegContactPoints.Num();
}
UPrimitiveComponent* ACMCentipedePawn::GetAggressiveMovementBody() const
{
    return GetLeadingBody();
}
FVector ACMCentipedePawn::GetAggressiveNavigationReferenceLocation() const
{
    const UBoxComponent* LeadingBody = GetLeadingBody();

    return LeadingBody ? LeadingBody->GetComponentLocation() : GetActorLocation();
}

// 머리와 꼬리의 완전한 경로 길이를 비교해 목적지에 유리한 진행 선두를 선택한다.
void ACMCentipedePawn::PrepareAggressivePathMove(FVector WorldGoal)
{
    if (!HasAuthority() || !PathMovement || !HeadBody || BodySegments.IsEmpty())
        return;

    float HeadPathLength = 0.0f;
    float TailPathLength = 0.0f;
    const bool bHasHeadPath = PathMovement->CalculateNavigationPathLength(HeadBody->GetComponentLocation(), WorldGoal, HeadPathLength);
    const bool bHasTailPath = PathMovement->CalculateNavigationPathLength(BodySegments.Last()->GetComponentLocation(), WorldGoal, TailPathLength);
    if (!bHasHeadPath && !bHasTailPath)
        return;

    const bool bUseTail = bHasTailPath && (!bHasHeadPath || TailPathLength < HeadPathLength);
    SetTailLeading(bUseTail);
}

// 경로 목표를 정리하고 Centipede 이동 완료 결과를 구독자에게 전달한다.
void ACMCentipedePawn::HandleAggressivePathMoveCompleted(ECMAggressivePathMoveResult Result)
{
    if (MovementCommand)
        MovementCommand->ClearMovementGoal();
    OnPathMoveCompleted.Broadcast(Result);
}

void ACMCentipedePawn::ResetLegActuation()
{
    if (LegActuator)
        LegActuator->ResetCooldowns();
}

int32 ACMCentipedePawn::GetBodySegmentCount() const
{
    return BodySegments.Num();
}
int32 ACMCentipedePawn::GetJointCount() const
{
    return FMath::Max(BodySegments.Num() - 1, 0);
}
float ACMCentipedePawn::GetSegmentCenterSpacing() const
{
    return SegmentCenterSpacing;
}
float ACMCentipedePawn::GetHorizontalBendLimitDegrees() const
{
    return HorizontalBendLimitDegrees;
}
float ACMCentipedePawn::GetMaxPlanarSpeed() const
{
    return MaxPlanarSpeed;
}
UBoxComponent* ACMCentipedePawn::GetHeadBody() const
{
    return HeadBody;
}
UBoxComponent* ACMCentipedePawn::GetLeadingBody() const
{
    return bTailLeading && !BodySegments.IsEmpty() ? BodySegments.Last() : HeadBody;
}

FVector ACMCentipedePawn::GetLeadingTipLocation() const
{
    const UBoxComponent* LeadingBody = GetLeadingBody();
    if (!LeadingBody)
        return GetActorLocation();

    const FVector TravelForward = bTailLeading ? -LeadingBody->GetForwardVector() : LeadingBody->GetForwardVector();

    return LeadingBody->GetComponentLocation() + TravelForward * LeadingBody->GetScaledBoxExtent().X;
}

void ACMCentipedePawn::SetTailLeading(bool bInTailLeading)
{
    if (!HasAuthority() || bTailLeading == bInTailLeading)
        return;
    bTailLeading = bInTailLeading;
    bAligningLeadingEnd = false;
    TargetJointAnglesDegrees.Init(0.0f, GetJointCount());
    LastTargetUpdateTime = 0.0;
    ResetHeadTrail();
}

void ACMCentipedePawn::SwapLeadingEnd()
{
    SetTailLeading(!bTailLeading);
}
UCMAggressiveMovementCommandComponent* ACMCentipedePawn::GetMovementCommand() const
{
    return MovementCommand;
}
UCMAggressiveOmnidirectionalPathComponent* ACMCentipedePawn::GetPathMovement() const
{
    return PathMovement;
}
const TArray<TObjectPtr<UBoxComponent>>& ACMCentipedePawn::GetBodySegments() const
{
    return BodySegments;
}
const TArray<TObjectPtr<UPhysicsConstraintComponent>>& ACMCentipedePawn::GetSegmentConstraints() const
{
    return SegmentConstraints;
}
const TArray<TObjectPtr<UStaticMeshComponent>>& ACMCentipedePawn::GetBodyMeshes() const
{
    return BodyMeshes;
}
const TArray<TObjectPtr<UStaticMeshComponent>>& ACMCentipedePawn::GetLegMeshes() const
{
    return LegMeshes;
}

// 큰 방향 오차에서는 전 몸통을 회전시키고 정렬 완료 뒤 정책 다리 제어로 복귀한다.
bool ACMCentipedePawn::UpdateLeadingEndAlignment()
{
    UBoxComponent* LeadingBody = GetLeadingBody();
    if (!HasAuthority() || !LeadingBody || !PathMovement || !PathMovement->IsPathMoving() || !MovementCommand || !MovementCommand->HasMovementGoal())
    {
        bAligningLeadingEnd = false;
        return false;
    }

    FVector GoalDirection = MovementCommand->GetMovementGoal().WorldLocation - LeadingBody->GetComponentLocation();
    GoalDirection.Z = 0.0f;
    if (!GoalDirection.Normalize())
    {
        bAligningLeadingEnd = false;
        return false;
    }

    FVector TravelForward = bTailLeading ? -LeadingBody->GetForwardVector() : LeadingBody->GetForwardVector();
    TravelForward.Z = 0.0f;
    TravelForward.Normalize();
    const float YawErrorDegrees = FMath::FindDeltaAngleDegrees(TravelForward.Rotation().Yaw, GoalDirection.Rotation().Yaw);
    const float AbsoluteYawError = FMath::Abs(YawErrorDegrees);
    // 시작·종료 임계값을 분리해 경계 각도에서 정렬 상태가 반복 전환되는 것을 막는다.
    if (!bAligningLeadingEnd && AbsoluteYawError < FMath::Max(LeadingAlignmentStartAngleDegrees, 0.0f))
        return false;
    if (bAligningLeadingEnd && AbsoluteYawError <= FMath::Max(LeadingAlignmentStopAngleDegrees, 0.0f))
    {
        bAligningLeadingEnd = false;
        for (UBoxComponent* Segment : BodySegments)
        {
            if (!Segment)
                continue;
            FVector AngularVelocity = Segment->GetPhysicsAngularVelocityInRadians();
            AngularVelocity.Z = 0.0f;
            Segment->SetPhysicsAngularVelocityInRadians(AngularVelocity);
        }
        return false;
    }

    bAligningLeadingEnd = true;
    const float MaximumAngularSpeed = FMath::Max(LeadingAlignmentAngularSpeedDegreesPerSecond, 0.0f);
    const float AngularSpeedDegrees = FMath::Clamp(YawErrorDegrees * FMath::Max(LeadingAlignmentAngularSpeedGain, 0.0f), -MaximumAngularSpeed, MaximumAngularSpeed);
    const float LastSegmentScale = FMath::Clamp(TrailingAlignmentAngularSpeedScale, 0.0f, 1.0f);
    for (int32 TraversalIndex = 0; TraversalIndex < BodySegments.Num(); ++TraversalIndex)
    {
        const int32 PhysicalIndex = bTailLeading ? BodySegments.Num() - 1 - TraversalIndex : TraversalIndex;
        UBoxComponent* Segment = BodySegments[PhysicalIndex];
        if (!Segment)
            continue;

        const float Alpha = BodySegments.Num() > 1 ? static_cast<float>(TraversalIndex) / static_cast<float>(BodySegments.Num() - 1) : 0.0f;
        FVector AngularVelocity = Segment->GetPhysicsAngularVelocityInRadians();
        AngularVelocity.Z = FMath::DegreesToRadians(AngularSpeedDegrees * FMath::Lerp(1.0f, LastSegmentScale, Alpha));
        Segment->SetPhysicsAngularVelocityInRadians(AngularVelocity);
        Segment->WakeAllRigidBodies();
    }
    return true;
}

// 선두 이동 궤적과 현재 목표를 이용해 각 관절의 부드러운 추종 각도를 갱신한다.
void ACMCentipedePawn::RefreshJointTargets()
{
    UBoxComponent* LeadingBody = GetLeadingBody();
    if (!LeadingBody || BodySegments.Num() != CMCentipedeBody::SegmentCount)
        return;

    UpdateHeadTrail();
    // 선두 뒤의 누적 궤적을 세그먼트 간격으로 샘플링해 목표 중심선을 만든다.
    TArray<FVector, TInlineAllocator<CMCentipedeBody::SegmentCount>> TargetCenters;
    TargetCenters.SetNum(CMCentipedeBody::SegmentCount);
    TargetCenters[0] = LeadingBody->GetComponentLocation();
    for (int32 SegmentIndex = 1; SegmentIndex < TargetCenters.Num(); ++SegmentIndex)
        TargetCenters[SegmentIndex] = SampleHeadTrail(SegmentCenterSpacing * SegmentIndex);

    TArray<float, TInlineAllocator<CMCentipedeBody::SegmentCount>> TargetYaws;
    TargetYaws.SetNum(CMCentipedeBody::SegmentCount);
    for (int32 SegmentIndex = 0; SegmentIndex < TargetYaws.Num(); ++SegmentIndex)
    {
        const FVector ForwardPoint = SegmentIndex == 0 ? TargetCenters[0] : TargetCenters[SegmentIndex - 1];
        const FVector RearPoint = SegmentIndex + 1 < TargetCenters.Num() ? TargetCenters[SegmentIndex + 1] : TargetCenters[SegmentIndex];
        FVector Direction = ForwardPoint - RearPoint;
        Direction.Z = 0.0f;
        const int32 PhysicalSegmentIndex = bTailLeading ? CMCentipedeBody::SegmentCount - 1 - SegmentIndex : SegmentIndex;
        TargetYaws[SegmentIndex] = Direction.IsNearlyZero() ? BodySegments[PhysicalSegmentIndex]->GetComponentRotation().Yaw : Direction.Rotation().Yaw;
    }

    // 첫 관절은 경로 목표 쪽으로 제한된 각도만 추가해 몸통 전체가 코너를 선행하게 한다.
    if (MovementCommand && MovementCommand->HasMovementGoal())
    {
        FVector GoalDirection = MovementCommand->GetMovementGoal().WorldLocation - TargetCenters[0];
        GoalDirection.Z = 0.0f;
        if (!GoalDirection.IsNearlyZero())
        {
            const float GoalYawDelta = FMath::FindDeltaAngleDegrees(TargetYaws[0], GoalDirection.Rotation().Yaw);
            TargetYaws[0] += FMath::Clamp(GoalYawDelta, -OperatingBendLimitDegrees, OperatingBendLimitDegrees);
        }
    }

    TArray<float, TInlineAllocator<CMCentipedeBody::JointCount>> DesiredAngles;
    DesiredAngles.Init(0.0f, CMCentipedeBody::JointCount);
    const UWorld* World = GetWorld();
    const float TimeSeconds = World ? World->GetTimeSeconds() : 0.0f;
    for (int32 TraversalJointIndex = 0; TraversalJointIndex < CMCentipedeBody::JointCount; ++TraversalJointIndex)
    {
        float DesiredAngle = FMath::FindDeltaAngleDegrees(TargetYaws[TraversalJointIndex], TargetYaws[TraversalJointIndex + 1]);
        // 학습 중에는 서로 다른 곡률 궤적을 반복해 직선 이동에만 과적합되지 않게 한다.
        if (TrainingCurveProfile > 0)
        {
            const float Amplitude = TrainingCurveProfile == 1 ? 20.0f : 35.0f;
            const float DirectionSign = TrainingCurveProfile == 3 ? -1.0f : 1.0f;
            DesiredAngle += DirectionSign * Amplitude * FMath::Sin(TimeSeconds * 1.6f - TraversalJointIndex * HALF_PI);
        }
        // 꼬리 진행 시 논리적 진행 순서를 실제 관절 인덱스와 회전 부호로 다시 매핑한다.
        const int32 PhysicalJointIndex = bTailLeading ? CMCentipedeBody::JointCount - 1 - TraversalJointIndex : TraversalJointIndex;
        DesiredAngles[PhysicalJointIndex] = FMath::Clamp(bTailLeading ? -DesiredAngle : DesiredAngle, -OperatingBendLimitDegrees, OperatingBendLimitDegrees);
    }

    // 목표 각도를 속도 제한 보간해 관절 드라이브의 급격한 방향 반전을 줄인다.
    const double CurrentTime = World ? World->GetTimeSeconds() : 0.0;
    const float DeltaSeconds = LastTargetUpdateTime > 0.0 ? static_cast<float>(CurrentTime - LastTargetUpdateTime) : 0.0f;
    for (int32 JointIndex = 0; JointIndex < DesiredAngles.Num(); ++JointIndex)
    {
        if (DeltaSeconds > 0.0f)
        {
            TargetJointAnglesDegrees[JointIndex] = FMath::FInterpConstantTo(TargetJointAnglesDegrees[JointIndex], DesiredAngles[JointIndex], DeltaSeconds, TargetJointAngleRateDegreesPerSecond);
        }
        else
        {
            TargetJointAnglesDegrees[JointIndex] = DesiredAngles[JointIndex];
        }
    }
    LastTargetUpdateTime = CurrentTime;
}

// 학습 관측과 보상 계산에 사용할 현재·목표 관절각 및 상대 각속도를 수집한다.
void ACMCentipedePawn::GetJointState(TArray<float>& OutCurrentAnglesDegrees, TArray<float>& OutTargetAnglesDegrees, TArray<float>& OutAngularVelocitiesRadians) const
{
    const int32 JointCount = GetJointCount();
    OutCurrentAnglesDegrees.SetNumZeroed(JointCount);
    OutTargetAnglesDegrees.Init(0.0f, JointCount);
    OutAngularVelocitiesRadians.SetNumZeroed(JointCount);
    for (int32 JointIndex = 0; JointIndex < JointCount; ++JointIndex)
    {
        const UBoxComponent* Front = BodySegments[JointIndex];
        const UBoxComponent* Rear = BodySegments[JointIndex + 1];
        if (!Front || !Rear)
            continue;
        OutCurrentAnglesDegrees[JointIndex] = FMath::FindDeltaAngleDegrees(Front->GetComponentRotation().Yaw, Rear->GetComponentRotation().Yaw);
        if (TargetJointAnglesDegrees.IsValidIndex(JointIndex))
            OutTargetAnglesDegrees[JointIndex] = TargetJointAnglesDegrees[JointIndex];
        OutAngularVelocitiesRadians[JointIndex] = Rear->GetPhysicsAngularVelocityInRadians().Z - Front->GetPhysicsAngularVelocityInRadians().Z;
    }
}

float ACMCentipedePawn::GetMeanNormalizedJointError() const
{
    TArray<float> CurrentAngles;
    TArray<float> Targets;
    TArray<float> AngularVelocities;
    GetJointState(CurrentAngles, Targets, AngularVelocities);
    if (CurrentAngles.IsEmpty())
        return 0.0f;
    float ErrorSum = 0.0f;
    for (int32 JointIndex = 0; JointIndex < CurrentAngles.Num(); ++JointIndex)
        ErrorSum += FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentAngles[JointIndex], Targets[JointIndex]));

    return FMath::Clamp(ErrorSum / (CurrentAngles.Num() * FMath::Max(OperatingBendLimitDegrees, 1.0f)), 0.0f, 2.0f);
}

float ACMCentipedePawn::GetMinimumSegmentUprightDot() const
{
    float MinimumDot = 1.0f;
    for (const UBoxComponent* Segment : BodySegments)
        if (Segment)
            MinimumDot = FMath::Min(MinimumDot, FVector::DotProduct(Segment->GetUpVector(), FVector::UpVector));

    return MinimumDot;
}

void ACMCentipedePawn::SetTrainingCurveProfile(int32 ProfileIndex)
{
    TrainingCurveProfile = FMath::Clamp(ProfileIndex, 0, 3);
}

// 학습 에피소드 시작 시 모든 세그먼트를 일렬 배치하고 물리·관절·다리 상태를 초기화한다.
void ACMCentipedePawn::ResetArticulatedBody(const FTransform& HeadTransform)
{
    bTailLeading = false;
    bAligningLeadingEnd = false;
    const FVector Forward = HeadTransform.GetRotation().GetForwardVector();
    for (int32 SegmentIndex = 0; SegmentIndex < BodySegments.Num(); ++SegmentIndex)
    {
        UBoxComponent* Segment = BodySegments[SegmentIndex];
        if (!Segment)
            continue;
        const FTransform SegmentTransform(HeadTransform.GetRotation(), HeadTransform.GetLocation() - Forward * SegmentCenterSpacing * SegmentIndex, FVector::OneVector);
        Segment->SetWorldTransform(SegmentTransform, false, nullptr, ETeleportType::TeleportPhysics);
        Segment->SetPhysicsLinearVelocity(FVector::ZeroVector);
        Segment->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
        Segment->WakeAllRigidBodies();
    }
    TargetJointAnglesDegrees.Init(0.0f, GetJointCount());
    LastTargetUpdateTime = 0.0;
    ResetLegActuation();
    ResetHeadTrail();
}

// 모든 몸통 세그먼트의 선속도와 각속도를 제거해 관절 몸체를 즉시 정지한다.
void ACMCentipedePawn::StopArticulatedBodyMotion()
{
    for (UBoxComponent* Segment : BodySegments)
    {
        if (!Segment)
            continue;
        Segment->SetPhysicsLinearVelocity(FVector::ZeroVector);
        Segment->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
    }
}

UBoxComponent* ACMCentipedePawn::AddBodySegment(int32 SegmentIndex, UStaticMesh* CubeMesh)
{
    UBoxComponent* Segment = HeadBody;
    if (SegmentIndex > 0)
    {
        Segment = CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("Segment%02dBody"), SegmentIndex));
        Segment->SetupAttachment(HeadBody);
        Segment->SetRelativeLocation(FVector(-SegmentCenterSpacing * SegmentIndex, 0.0f, 0.0f));
        Segment->SetBoxExtent(FVector(BodyLength, BodyWidth, BodyHeight) * 0.5f);
        Segment->SetCollisionProfileName(TEXT("PhysicsActor"));
        Segment->SetSimulatePhysics(false);
        Segment->SetIsReplicated(true);
        Segment->SetCanEverAffectNavigation(false);
        Segment->BodyInstance.bAutoWeld = false;
        BodySegments.Add(Segment);
    }

    UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Segment%02dMesh"), SegmentIndex));
    Mesh->SetupAttachment(Segment);
    Mesh->SetStaticMesh(CubeMesh);
    Mesh->SetRelativeScale3D(FVector(BodyLength, BodyWidth, BodyHeight) / 100.0f);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCanEverAffectNavigation(false);
    BodyMeshes.Add(Mesh);

    return Segment;
}

void ACMCentipedePawn::AddLeg(int32 SegmentIndex, bool bLeftLeg, UStaticMesh* CubeMesh)
{
    UBoxComponent* Segment = BodySegments[SegmentIndex];
    const TCHAR* SideName = bLeftLeg ? TEXT("Left") : TEXT("Right");
    const float SideSign = bLeftLeg ? -1.0f : 1.0f;
    const FVector ContactLocation(0.0f, SideSign * CMCentipedeBody::LegLateralOffset, CMCentipedeBody::GroundContactHeight);
    const FVector LegCenter = ContactLocation + FVector(0.0f, 0.0f, CMCentipedeBody::LegHalfHeight);

    UBoxComponent* Collision = CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("Segment%02d%sLegCollision"), SegmentIndex, SideName));
    Collision->SetupAttachment(Segment);
    Collision->SetRelativeLocation(LegCenter);
    Collision->SetBoxExtent(FVector(10.0f, 10.0f, CMCentipedeBody::LegHalfHeight));
    Collision->SetCollisionProfileName(TEXT("PhysicsActor"));
    Collision->SetSimulatePhysics(false);
    Collision->SetCanEverAffectNavigation(false);
    Collision->BodyInstance.bAutoWeld = true;
    LegCollisions.Add(Collision);

    UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Segment%02d%sLegMesh"), SegmentIndex, SideName));
    Mesh->SetupAttachment(Segment);
    Mesh->SetStaticMesh(CubeMesh);
    Mesh->SetRelativeLocation(LegCenter);
    Mesh->SetRelativeScale3D(FVector(0.2f, 0.2f, 0.4f));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCanEverAffectNavigation(false);
    Mesh->SetHiddenInGame(true);
    LegMeshes.Add(Mesh);

    USceneComponent* Contact = CreateDefaultSubobject<USceneComponent>(*FString::Printf(TEXT("Segment%02d%sLegContact"), SegmentIndex, SideName));
    Contact->SetupAttachment(Segment);
    Contact->SetRelativeLocation(ContactLocation);
    LegContactPoints.Add(Contact);
    LegBodies.Add(Segment);

    const FVector OutwardDirection(0.0f, SideSign, 0.0f);
    const float PhaseOffset = static_cast<float>((SegmentIndex * 2 + (bLeftLeg ? 0 : 1)) % 4) / 4.0f;
    UCMAIProceduralLegComponent* ProceduralLeg = CreateDefaultSubobject<UCMAIProceduralLegComponent>(*FString::Printf(TEXT("Segment%02d%sProceduralLeg"), SegmentIndex, SideName));
    ProceduralLeg->SetupAttachment(Segment);
    ProceduralLeg->SetRelativeLocation(ContactLocation - OutwardDirection * 30.0f + FVector::UpVector * 130.0f);
    ProceduralLeg->Configure(Contact, OutwardDirection, PhaseOffset, 1.8f);
    ProceduralLegMeshes.Add(ProceduralLeg);
}

void ACMCentipedePawn::ApplyBodySettings()
{
    for (UBoxComponent* Segment : BodySegments)
    {
        if (!Segment)
            continue;
        if (Segment != HeadBody)
            Segment->SetSimulatePhysics(true);
        Segment->SetEnableGravity(bUseGravity);
        Segment->SetLinearDamping(FMath::Max(LinearDamping, 0.0f));
        Segment->SetAngularDamping(FMath::Max(AngularDamping, 0.0f));
        Segment->SetMassOverrideInKg(NAME_None, FMath::Max(BodyMassPerSegmentKg, 0.1f), true);
        Segment->BodyInstance.bLockXRotation = true;
        Segment->BodyInstance.bLockYRotation = true;
        Segment->BodyInstance.bLockZRotation = false;
        Segment->BodyInstance.SetDOFLock(EDOFMode::SixDOF);
    }
}

// 인접 몸통을 제한된 수평 굽힘과 감쇠 드라이브를 가진 물리 관절로 연결한다.
void ACMCentipedePawn::CreateSegmentConstraints()
{
    DestroySegmentConstraints();
    for (int32 JointIndex = 0; JointIndex < BodySegments.Num() - 1; ++JointIndex)
    {
        UBoxComponent* Front = BodySegments[JointIndex];
        UBoxComponent* Rear = BodySegments[JointIndex + 1];
        if (!Front || !Rear || !Front->IsSimulatingPhysics() || !Rear->IsSimulatingPhysics())
            continue;

        UPhysicsConstraintComponent* Constraint = NewObject<UPhysicsConstraintComponent>(this, *FString::Printf(TEXT("SegmentConstraint%02d"), JointIndex));
        if (!Constraint)
            continue;
        Constraint->SetupAttachment(HeadBody);
        Constraint->SetWorldLocation((Front->GetComponentLocation() + Rear->GetComponentLocation()) * 0.5f);
        Constraint->SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
        Constraint->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
        Constraint->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
        Constraint->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, HorizontalBendLimitDegrees);
        Constraint->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Limited, 20.0f);
        Constraint->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Limited, 5.0f);
        Constraint->SetAngularDriveMode(EAngularDriveMode::TwistAndSwing);
        Constraint->SetOrientationDriveTwistAndSwing(false, false);
        Constraint->SetAngularVelocityDriveTwistAndSwing(false, ConstraintVelocityDamping > 0.0f);
        Constraint->SetAngularVelocityTarget(FVector::ZeroVector);
        Constraint->SetAngularDriveParams(0.0f, ConstraintVelocityDamping, 0.0f);
        Constraint->SetAngularDriveAccelerationMode(true);
        Constraint->SetDisableCollision(true);
        AddInstanceComponent(Constraint);
        Constraint->SetProjectionEnabled(false);
        Constraint->RegisterComponent();
        Constraint->SetConstrainedComponents(Front, NAME_None, Rear, NAME_None);
        SegmentConstraints.Add(Constraint);
    }
}

void ACMCentipedePawn::DestroySegmentConstraints()
{
    for (UPhysicsConstraintComponent* Constraint : SegmentConstraints)
        if (Constraint)
            Constraint->DestroyComponent();
    SegmentConstraints.Reset();
}

// 현재 진행 선두 뒤에 초기 직선 궤적을 만들어 관절 목표 샘플링을 준비한다.
void ACMCentipedePawn::ResetHeadTrail()
{
    HeadTrailPoints.Reset();
    UBoxComponent* LeadingBody = GetLeadingBody();
    if (!LeadingBody)
        return;
    const FVector Start = LeadingBody->GetComponentLocation();
    const FVector TravelForward = bTailLeading ? -LeadingBody->GetForwardVector() : LeadingBody->GetForwardVector();
    const FVector Backward = -TravelForward;
    const float RequiredLength = SegmentCenterSpacing * CMCentipedeBody::SegmentCount;
    for (float Distance = 0.0f; Distance <= RequiredLength + CMCentipedeBody::TrailSampleDistance; Distance += CMCentipedeBody::TrailSampleDistance)
        HeadTrailPoints.Add(Start + Backward * Distance);
}

// 선두 이동을 일정 거리 간격으로 기록하고 몸통 길이를 넘는 오래된 궤적을 제거한다.
void ACMCentipedePawn::UpdateHeadTrail()
{
    UBoxComponent* LeadingBody = GetLeadingBody();
    if (!LeadingBody)
        return;
    if (HeadTrailPoints.IsEmpty())
    {
        ResetHeadTrail();

        return;
    }

    const FVector Current = LeadingBody->GetComponentLocation();
    if (FVector::DistSquared2D(Current, HeadTrailPoints[0]) >= FMath::Square(CMCentipedeBody::TrailSampleDistance))
        HeadTrailPoints.Insert(Current, 0);
    else
        HeadTrailPoints[0] = Current;

    const float MaximumLength = SegmentCenterSpacing * CMCentipedeBody::SegmentCount + CMCentipedeBody::TrailSampleDistance;
    float Accumulated = 0.0f;
    int32 KeepCount = HeadTrailPoints.Num();
    for (int32 PointIndex = 1; PointIndex < HeadTrailPoints.Num(); ++PointIndex)
    {
        Accumulated += FVector::Dist2D(HeadTrailPoints[PointIndex - 1], HeadTrailPoints[PointIndex]);
        if (Accumulated > MaximumLength)
        {
            KeepCount = PointIndex + 1;
            break;
        }
    }
    if (HeadTrailPoints.Num() > KeepCount)
        HeadTrailPoints.RemoveAt(KeepCount, HeadTrailPoints.Num() - KeepCount, EAllowShrinking::No);
}

// 선두에서 지정한 누적 거리만큼 뒤에 있는 궤적 위치를 선형 보간한다.
FVector ACMCentipedePawn::SampleHeadTrail(float DistanceBehindHead) const
{
    if (HeadTrailPoints.IsEmpty())
    {
        const UBoxComponent* LeadingBody = GetLeadingBody();

        return LeadingBody ? LeadingBody->GetComponentLocation() : GetActorLocation();
    }

    float Accumulated = 0.0f;
    for (int32 PointIndex = 1; PointIndex < HeadTrailPoints.Num(); ++PointIndex)
    {
        const float SegmentLength = FVector::Dist2D(HeadTrailPoints[PointIndex - 1], HeadTrailPoints[PointIndex]);
        if (Accumulated + SegmentLength >= DistanceBehindHead && SegmentLength > UE_SMALL_NUMBER)
            return FMath::Lerp(HeadTrailPoints[PointIndex - 1], HeadTrailPoints[PointIndex], (DistanceBehindHead - Accumulated) / SegmentLength);
        Accumulated += SegmentLength;
    }
    return HeadTrailPoints.Last();
}
