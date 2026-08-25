#include "Aggressive/Centipede/Learning/CMCentipedeLearningInteractor.h"

#include "Aggressive/Centipede/CMCentipedePawn.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "LearningAgentsActions.h"
#include "LearningAgentsManager.h"
#include "LearningAgentsObservations.h"
#include "Aggressive/Common/Movement/CMAggressiveMovementCommandComponent.h"

namespace
{
    const FName CentipedeGoalDirectionTag(TEXT("GoalDirection"));
    const FName CentipedeHeadVelocityTag(TEXT("HeadPlanarVelocity"));
    const FName CentipedeHeadYawVelocityTag(TEXT("HeadYawAngularVelocity"));
    const FName CentipedeJointAnglesTag(TEXT("JointAngles"));
    const FName CentipedeTargetJointAnglesTag(TEXT("TargetJointAngles"));
    const FName CentipedeJointErrorsTag(TEXT("JointAngleErrors"));
    const FName CentipedeJointVelocitiesTag(TEXT("JointAngularVelocities"));
    const FName CentipedeSegmentVelocitiesTag(TEXT("SegmentPlanarVelocities"));
    const FName CentipedeObservationTag(TEXT("CentipedeObservation"));
    const FName CentipedeLegActionsTag(TEXT("CentipedeLegActivations"));

    constexpr int32 CentipedeLegCount = 8;
    constexpr int32 CentipedeSegmentCount = 4;
    constexpr int32 CentipedeJointCount = 3;
    constexpr int32 CentipedeSegmentVelocityCount = 8;
    constexpr float CentipedeVelocityScale = 240.0f;
    constexpr float CentipedeYawVelocityScale = 5.0f;
    constexpr float CentipedeJointAngleScale = 100.0f;
    constexpr float CentipedeJointVelocityScale = 5.0f;

    ECMAggressiveMoveDirection ReverseDirection(ECMAggressiveMoveDirection Direction)
    {
        if (Direction == ECMAggressiveMoveDirection::None)
            return Direction;
        return static_cast<ECMAggressiveMoveDirection>((static_cast<uint8>(Direction) + 4) % 8);
    }
}

UCMCentipedeLearningInteractor* UCMCentipedeLearningInteractor::MakeCentipedeInteractor(ULearningAgentsManager*& InManager, FName Name)
{
    if (!InManager)
        return nullptr;
    UCMCentipedeLearningInteractor* Interactor = NewObject<UCMCentipedeLearningInteractor>(InManager, StaticClass(), MakeUniqueObjectName(InManager, StaticClass(), Name, EUniqueObjectNameOptions::GloballyUnique));
    if (!Interactor)
        return nullptr;
    Interactor->ActiveLegIndicesScratch.Reserve(CentipedeLegCount);
    Interactor->SetupInteractor(InManager);
    return Interactor->IsSetup() ? Interactor : nullptr;
}

void UCMCentipedeLearningInteractor::SpecifyAgentObservation_Implementation(FLearningAgentsObservationSchemaElement& OutObservationSchemaElement, ULearningAgentsObservationSchema* InObservationSchema)
{
    const FName Names[] = {CentipedeGoalDirectionTag, CentipedeHeadVelocityTag, CentipedeHeadYawVelocityTag, CentipedeJointAnglesTag, CentipedeTargetJointAnglesTag, CentipedeJointErrorsTag, CentipedeJointVelocitiesTag, CentipedeSegmentVelocitiesTag};
    const FLearningAgentsObservationSchemaElement Elements[] = {
        ULearningAgentsObservations::SpecifyEnumObservation(InObservationSchema, StaticEnum<ECMAggressiveMoveDirection>(), CentipedeGoalDirectionTag),
        ULearningAgentsObservations::SpecifyContinuousObservation(InObservationSchema, 2, CentipedeVelocityScale, CentipedeHeadVelocityTag),
        ULearningAgentsObservations::SpecifyFloatObservation(InObservationSchema, CentipedeYawVelocityScale, CentipedeHeadYawVelocityTag),
        ULearningAgentsObservations::SpecifyContinuousObservation(InObservationSchema, CentipedeJointCount, CentipedeJointAngleScale, CentipedeJointAnglesTag),
        ULearningAgentsObservations::SpecifyContinuousObservation(InObservationSchema, CentipedeJointCount, CentipedeJointAngleScale, CentipedeTargetJointAnglesTag),
        ULearningAgentsObservations::SpecifyContinuousObservation(InObservationSchema, CentipedeJointCount, CentipedeJointAngleScale, CentipedeJointErrorsTag),
        ULearningAgentsObservations::SpecifyContinuousObservation(InObservationSchema, CentipedeJointCount, CentipedeJointVelocityScale, CentipedeJointVelocitiesTag),
        ULearningAgentsObservations::SpecifyContinuousObservation(InObservationSchema, CentipedeSegmentVelocityCount, CentipedeVelocityScale, CentipedeSegmentVelocitiesTag)
    };
    OutObservationSchemaElement = ULearningAgentsObservations::SpecifyStructObservationFromArrayViews(InObservationSchema, MakeArrayView(Names), MakeArrayView(Elements), CentipedeObservationTag);
}

void UCMCentipedeLearningInteractor::GatherAgentObservation_Implementation(FLearningAgentsObservationObjectElement& OutObservationObjectElement, ULearningAgentsObservationObject* InObservationObject, int32 AgentId)
{
    ACMCentipedePawn* Agent = Cast<ACMCentipedePawn>(GetAgent(AgentId));
    ECMAggressiveMoveDirection GoalDirection = ECMAggressiveMoveDirection::None;
    float HeadVelocityValues[2] = {0.0f, 0.0f};
    float HeadYawVelocity = 0.0f;
    TArray<float> JointAngles;
    TArray<float> TargetJointAngles;
    TArray<float> JointVelocities;
    TArray<float> JointErrors;
    TArray<float> SegmentVelocities;
    JointAngles.Init(0.0f, CentipedeJointCount);
    TargetJointAngles.Init(0.0f, CentipedeJointCount);
    JointVelocities.Init(0.0f, CentipedeJointCount);
    JointErrors.Init(0.0f, CentipedeJointCount);
    SegmentVelocities.Init(0.0f, CentipedeSegmentVelocityCount);

    if (Agent)
    {
        Agent->RefreshJointTargets();
        Agent->GetJointState(JointAngles, TargetJointAngles, JointVelocities);
        if (Agent->IsTailLeading())
        {
            const TArray<float> PhysicalJointAngles = JointAngles;
            const TArray<float> PhysicalTargetJointAngles = TargetJointAngles;
            const TArray<float> PhysicalJointVelocities = JointVelocities;
            for (int32 JointIndex = 0; JointIndex < CentipedeJointCount; ++JointIndex)
            {
                const int32 PhysicalJointIndex = CentipedeJointCount - 1 - JointIndex;
                JointAngles[JointIndex] = -PhysicalJointAngles[PhysicalJointIndex];
                TargetJointAngles[JointIndex] = -PhysicalTargetJointAngles[PhysicalJointIndex];
                JointVelocities[JointIndex] = -PhysicalJointVelocities[PhysicalJointIndex];
            }
        }
        for (int32 JointIndex = 0; JointIndex < CentipedeJointCount; ++JointIndex)
            JointErrors[JointIndex] = FMath::FindDeltaAngleDegrees(JointAngles[JointIndex], TargetJointAngles[JointIndex]);

        UBoxComponent* Head = Agent->GetLeadingBody();
        UCMAggressiveMovementCommandComponent* Command = Agent->GetMovementCommand();
        if (Command)
            GoalDirection = Command->RefreshGoalDirection();
        if (Agent->IsTailLeading())
            GoalDirection = ReverseDirection(GoalDirection);
        if (Head)
        {
            const FQuat HeadYaw(FVector::UpVector, FMath::DegreesToRadians(Head->GetComponentRotation().Yaw));
            FVector LocalVelocity = HeadYaw.UnrotateVector(Head->GetPhysicsLinearVelocity());
            if (Agent->IsTailLeading())
                LocalVelocity *= -1.0f;
            HeadVelocityValues[0] = LocalVelocity.X;
            HeadVelocityValues[1] = LocalVelocity.Y;
            HeadYawVelocity = Head->GetPhysicsAngularVelocityInRadians().Z;
        }

        const TArray<TObjectPtr<UBoxComponent>>& Segments = Agent->GetBodySegments();
        for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num() && SegmentIndex < CentipedeSegmentCount; ++SegmentIndex)
        {
            const int32 PhysicalSegmentIndex = Agent->IsTailLeading() ? CentipedeSegmentCount - 1 - SegmentIndex : SegmentIndex;
            UBoxComponent* Segment = Segments[PhysicalSegmentIndex];
            if (!Segment)
                continue;
            const FQuat SegmentYaw(FVector::UpVector, FMath::DegreesToRadians(Segment->GetComponentRotation().Yaw));
            FVector LocalVelocity = SegmentYaw.UnrotateVector(Segment->GetPhysicsLinearVelocity());
            if (Agent->IsTailLeading())
                LocalVelocity *= -1.0f;
            SegmentVelocities[SegmentIndex * 2] = LocalVelocity.X;
            SegmentVelocities[SegmentIndex * 2 + 1] = LocalVelocity.Y;
        }
    }

    const FName Names[] = {CentipedeGoalDirectionTag, CentipedeHeadVelocityTag, CentipedeHeadYawVelocityTag, CentipedeJointAnglesTag, CentipedeTargetJointAnglesTag, CentipedeJointErrorsTag, CentipedeJointVelocitiesTag, CentipedeSegmentVelocitiesTag};
    const FLearningAgentsObservationObjectElement Elements[] = {
        ULearningAgentsObservations::MakeEnumObservation(InObservationObject, StaticEnum<ECMAggressiveMoveDirection>(), static_cast<uint8>(GoalDirection), CentipedeGoalDirectionTag),
        ULearningAgentsObservations::MakeContinuousObservationFromArrayView(InObservationObject, MakeArrayView(HeadVelocityValues), CentipedeHeadVelocityTag),
        ULearningAgentsObservations::MakeFloatObservation(InObservationObject, HeadYawVelocity, CentipedeHeadYawVelocityTag),
        ULearningAgentsObservations::MakeContinuousObservationFromArrayView(InObservationObject, JointAngles, CentipedeJointAnglesTag),
        ULearningAgentsObservations::MakeContinuousObservationFromArrayView(InObservationObject, TargetJointAngles, CentipedeTargetJointAnglesTag),
        ULearningAgentsObservations::MakeContinuousObservationFromArrayView(InObservationObject, JointErrors, CentipedeJointErrorsTag),
        ULearningAgentsObservations::MakeContinuousObservationFromArrayView(InObservationObject, JointVelocities, CentipedeJointVelocitiesTag),
        ULearningAgentsObservations::MakeContinuousObservationFromArrayView(InObservationObject, SegmentVelocities, CentipedeSegmentVelocitiesTag)
    };
    OutObservationObjectElement = ULearningAgentsObservations::MakeStructObservationFromArrayViews(InObservationObject, MakeArrayView(Names), MakeArrayView(Elements), CentipedeObservationTag);
}

void UCMCentipedeLearningInteractor::SpecifyAgentAction_Implementation(FLearningAgentsActionSchemaElement& OutActionSchemaElement, ULearningAgentsActionSchema* InActionSchema)
{
    OutActionSchemaElement = ULearningAgentsActions::SpecifyInclusiveDiscreteAction(InActionSchema, CentipedeLegCount, {}, CentipedeLegActionsTag);
}

void UCMCentipedeLearningInteractor::PerformAgentAction_Implementation(const ULearningAgentsActionObject* InActionObject, const FLearningAgentsActionObjectElement& InActionObjectElement, int32 AgentId)
{
    ActiveLegIndicesScratch.Reset();
    if (!ULearningAgentsActions::GetInclusiveDiscreteAction(ActiveLegIndicesScratch, InActionObject, InActionObjectElement, CentipedeLegActionsTag))
        return;
    if (ACMCentipedePawn* Agent = Cast<ACMCentipedePawn>(GetAgent(AgentId)))
    {
        Agent->UpdateLeadingEndAlignment();
        if (Agent->IsTailLeading())
        {
            for (int32& LegIndex : ActiveLegIndicesScratch)
                if (LegIndex >= 0 && LegIndex < CentipedeLegCount)
                    LegIndex = CentipedeLegCount - 1 - LegIndex;
        }
        Agent->ActivateLegs(ActiveLegIndicesScratch);
    }
}
