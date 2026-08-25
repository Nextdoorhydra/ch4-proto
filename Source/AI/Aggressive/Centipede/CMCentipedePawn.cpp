#include "Aggressive/Centipede/CMCentipedePawn.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Aggressive/Common/Movement/CMAggressiveMovementCommandComponent.h"
#include "Aggressive/Common/Movement/CMAggressiveOmnidirectionalPathComponent.h"
#include "Aggressive/Common/Movement/CMAIFixedLegActuatorComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogCMCentipedePawn, Log, All);

namespace CMCentipedeBody
{
    constexpr int32 SegmentCount = 4;
    constexpr int32 JointCount = SegmentCount - 1;
    constexpr float GroundContactHeight = -100.0f;
    constexpr float LegHalfHeight = 20.0f;
    constexpr float LegLateralOffset = 90.0f;
    constexpr float TrailSampleDistance = 20.0f;
}

ACMCentipedePawn::ACMCentipedePawn()
{
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
    bReplicates = true;
    SetReplicateMovement(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));

    HeadBody = CreateDefaultSubobject<UBoxComponent>(TEXT("Segment00Body"));
    SetRootComponent(HeadBody);
    HeadBody->SetBoxExtent(FVector(BodyLength, BodyWidth, BodyHeight) * 0.5f);
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

    TargetJointAnglesDegrees.Init(0.0f, CMCentipedeBody::JointCount);
}

void ACMCentipedePawn::BeginPlay()
{
    Super::BeginPlay();
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

int32 ACMCentipedePawn::GetLegCount() const { return LegContactPoints.Num(); }
UPrimitiveComponent* ACMCentipedePawn::GetAggressiveMovementBody() const { return GetLeadingBody(); }
FVector ACMCentipedePawn::GetAggressiveNavigationReferenceLocation() const
{
    const UBoxComponent* LeadingBody = GetLeadingBody();
    return LeadingBody ? LeadingBody->GetComponentLocation() : GetActorLocation();
}

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
    UE_LOG(LogCMCentipedePawn, Display,
        TEXT("Centipede AI가 말단별 경로 길이를 비교해 %s를 선두로 선택했습니다. 머리=%s 꼬리=%s 목표=%s"),
        bUseTail ? TEXT("꼬리") : TEXT("머리"),
        bHasHeadPath ? *FString::Printf(TEXT("%.1fcm"), HeadPathLength) : TEXT("경로 없음"),
        bHasTailPath ? *FString::Printf(TEXT("%.1fcm"), TailPathLength) : TEXT("경로 없음"),
        *WorldGoal.ToCompactString());
}

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

int32 ACMCentipedePawn::GetBodySegmentCount() const { return BodySegments.Num(); }
int32 ACMCentipedePawn::GetJointCount() const { return FMath::Max(BodySegments.Num() - 1, 0); }
float ACMCentipedePawn::GetSegmentCenterSpacing() const { return SegmentCenterSpacing; }
float ACMCentipedePawn::GetHorizontalBendLimitDegrees() const { return HorizontalBendLimitDegrees; }
float ACMCentipedePawn::GetMaxPlanarSpeed() const { return MaxPlanarSpeed; }
UBoxComponent* ACMCentipedePawn::GetHeadBody() const { return HeadBody; }
UBoxComponent* ACMCentipedePawn::GetLeadingBody() const
{
    return bTailLeading && !BodySegments.IsEmpty() ? BodySegments.Last() : HeadBody;
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
UCMAggressiveMovementCommandComponent* ACMCentipedePawn::GetMovementCommand() const { return MovementCommand; }
UCMAggressiveOmnidirectionalPathComponent* ACMCentipedePawn::GetPathMovement() const { return PathMovement; }
const TArray<TObjectPtr<UBoxComponent>>& ACMCentipedePawn::GetBodySegments() const { return BodySegments; }
const TArray<TObjectPtr<UPhysicsConstraintComponent>>& ACMCentipedePawn::GetSegmentConstraints() const { return SegmentConstraints; }
const TArray<TObjectPtr<UStaticMeshComponent>>& ACMCentipedePawn::GetBodyMeshes() const { return BodyMeshes; }
const TArray<TObjectPtr<UStaticMeshComponent>>& ACMCentipedePawn::GetLegMeshes() const { return LegMeshes; }

bool ACMCentipedePawn::UpdateLeadingEndAlignment()
{
    UBoxComponent* LeadingBody = GetLeadingBody();
    if (!HasAuthority() || !LeadingBody || !PathMovement || !PathMovement->IsPathMoving()
        || !MovementCommand || !MovementCommand->HasMovementGoal())
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
    const float AngularSpeedDegrees = FMath::Clamp(
        YawErrorDegrees * FMath::Max(LeadingAlignmentAngularSpeedGain, 0.0f),
        -MaximumAngularSpeed,
        MaximumAngularSpeed);
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

void ACMCentipedePawn::RefreshJointTargets()
{
    UBoxComponent* LeadingBody = GetLeadingBody();
    if (!LeadingBody || BodySegments.Num() != CMCentipedeBody::SegmentCount)
        return;

    UpdateHeadTrail();
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
        if (TrainingCurveProfile > 0)
        {
            const float Amplitude = TrainingCurveProfile == 1 ? 20.0f : 35.0f;
            const float DirectionSign = TrainingCurveProfile == 3 ? -1.0f : 1.0f;
            DesiredAngle += DirectionSign * Amplitude * FMath::Sin(TimeSeconds * 1.6f - TraversalJointIndex * HALF_PI);
        }
        const int32 PhysicalJointIndex = bTailLeading ? CMCentipedeBody::JointCount - 1 - TraversalJointIndex : TraversalJointIndex;
        DesiredAngles[PhysicalJointIndex] = FMath::Clamp(bTailLeading ? -DesiredAngle : DesiredAngle, -OperatingBendLimitDegrees, OperatingBendLimitDegrees);
    }

    const double CurrentTime = World ? World->GetTimeSeconds() : 0.0;
    const float DeltaSeconds = LastTargetUpdateTime > 0.0 ? static_cast<float>(CurrentTime - LastTargetUpdateTime) : 0.0f;
    for (int32 JointIndex = 0; JointIndex < DesiredAngles.Num(); ++JointIndex)
    {
        TargetJointAnglesDegrees[JointIndex] = DeltaSeconds > 0.0f
            ? FMath::FInterpConstantTo(TargetJointAnglesDegrees[JointIndex], DesiredAngles[JointIndex], DeltaSeconds, TargetJointAngleRateDegreesPerSecond)
            : DesiredAngles[JointIndex];
    }
    LastTargetUpdateTime = CurrentTime;
}

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
    LegMeshes.Add(Mesh);

    USceneComponent* Contact = CreateDefaultSubobject<USceneComponent>(*FString::Printf(TEXT("Segment%02d%sLegContact"), SegmentIndex, SideName));
    Contact->SetupAttachment(Segment);
    Contact->SetRelativeLocation(ContactLocation);
    LegContactPoints.Add(Contact);
    LegBodies.Add(Segment);
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
