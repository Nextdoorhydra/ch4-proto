#include "Aggressive/Tetra/Learning/CMTetraLearningInteractor.h"

#include "Aggressive/Common/Core/CMAggressiveMovementAgent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "LearningAgentsActions.h"
#include "LearningAgentsManager.h"
#include "LearningAgentsObservations.h"
#include "LearningAgentsPolicy.h"
#include "Aggressive/Common/Movement/CMAggressiveAccelerationMovementComponent.h"
#include "Aggressive/Common/Movement/CMAggressiveMovementCommandComponent.h"

namespace
{
    const FName TetraGoalDirectionTag(TEXT("GoalDirection"));
    const FName TetraPlanarVelocityTag(TEXT("PlanarVelocity"));
    const FName TetraMovementObservationTag(TEXT("TetraMovementObservation"));
    const FName TetraAccelerationActionTag(TEXT("AccelerationAction"));
} // namespace

// Tetra AI 전용 관측과 행동 스키마를 가진 Interactor를 생성한다.
UCMTetraLearningInteractor* UCMTetraLearningInteractor::MakeTetraInteractor(ULearningAgentsManager*& InManager, FName Name)
{
    if (!InManager)
        return nullptr;

    const FName UniqueName = MakeUniqueObjectName(InManager, StaticClass(), Name, EUniqueObjectNameOptions::GloballyUnique);
    UCMTetraLearningInteractor* Interactor = NewObject<UCMTetraLearningInteractor>(InManager, StaticClass(), UniqueName);
    if (!Interactor)
        return nullptr;

    Interactor->SetupInteractor(InManager);

    return Interactor->IsSetup() ? Interactor : nullptr;
}

FLearningAgentsPolicySettings UCMTetraLearningInteractor::GetPolicySettings()
{
    FLearningAgentsPolicySettings Settings;
    Settings.HiddenLayerNum = 1;
    Settings.HiddenLayerSize = 32;
    Settings.MemoryCell = ELearningAgentsMemoryCell::NoMemoryCell;
    Settings.MemoryStateSize = 0;
    Settings.bUseParallelEvaluation = true;
    return Settings;
}

// 목표 방향과 현재 평면 속도를 관측으로 정의한다.
void UCMTetraLearningInteractor::SpecifyAgentObservation_Implementation(FLearningAgentsObservationSchemaElement& OutObservationSchemaElement, ULearningAgentsObservationSchema* InObservationSchema)
{
    const FName ElementNames[] = {TetraGoalDirectionTag, TetraPlanarVelocityTag};
    const FLearningAgentsObservationSchemaElement Elements[] = {ULearningAgentsObservations::SpecifyEnumObservation(InObservationSchema, StaticEnum<ECMAggressiveMoveDirection>(), TetraGoalDirectionTag), ULearningAgentsObservations::SpecifyContinuousObservation(InObservationSchema, 2, 1.0f, TetraPlanarVelocityTag)};
    OutObservationSchemaElement = ULearningAgentsObservations::SpecifyStructObservationFromArrayViews(InObservationSchema, MakeArrayView(ElementNames), MakeArrayView(Elements), TetraMovementObservationTag);
}

// Tetra AI의 현재 목표 방향과 몸통 기준 평면 속도를 수집한다.
void UCMTetraLearningInteractor::GatherAgentObservation_Implementation(FLearningAgentsObservationObjectElement& OutObservationObjectElement, ULearningAgentsObservationObject* InObservationObject, int32 AgentId)
{
    ECMAggressiveMoveDirection GoalDirection = ECMAggressiveMoveDirection::None;
    FVector LocalVelocity = FVector::ZeroVector;
    AActor* AgentActor = Cast<AActor>(GetAgent(AgentId));
    ICMAggressiveMovementAgent* MovementAgent = Cast<ICMAggressiveMovementAgent>(AgentActor);
    UPrimitiveComponent* Body = MovementAgent ? MovementAgent->GetAggressiveMovementBody() : nullptr;
    UCMAggressiveMovementCommandComponent* MovementCommand = AgentActor ? AgentActor->FindComponentByClass<UCMAggressiveMovementCommandComponent>() : nullptr;
    const UCMAggressiveAccelerationMovementComponent* Movement = AgentActor ? AgentActor->FindComponentByClass<UCMAggressiveAccelerationMovementComponent>() : nullptr;
    if (Body && MovementCommand && Movement)
    {
        GoalDirection = MovementCommand->RefreshGoalDirection();
        const FQuat BodyYaw(FVector::UpVector, FMath::DegreesToRadians(Body->GetComponentRotation().Yaw));
        const float SafeMaximumSpeed = FMath::Max(Movement->GetMaximumSpeed(), UE_KINDA_SMALL_NUMBER);
        LocalVelocity = BodyYaw.UnrotateVector(Body->GetPhysicsLinearVelocity()) / SafeMaximumSpeed;
    }

    const float PlanarVelocity[] = {static_cast<float>(LocalVelocity.X), static_cast<float>(LocalVelocity.Y)};
    const FName ElementNames[] = {TetraGoalDirectionTag, TetraPlanarVelocityTag};
    const FLearningAgentsObservationObjectElement Elements[] = {ULearningAgentsObservations::MakeEnumObservation(InObservationObject, StaticEnum<ECMAggressiveMoveDirection>(), static_cast<uint8>(GoalDirection), TetraGoalDirectionTag),
         ULearningAgentsObservations::MakeContinuousObservationFromArrayView(InObservationObject, MakeArrayView(PlanarVelocity), TetraPlanarVelocityTag)};
    OutObservationObjectElement = ULearningAgentsObservations::MakeStructObservationFromArrayViews(InObservationObject, MakeArrayView(ElementNames), MakeArrayView(Elements), TetraMovementObservationTag);
}

// 정책이 로컬 X/Y 가속도를 연속값으로 출력하도록 정의한다.
void UCMTetraLearningInteractor::SpecifyAgentAction_Implementation(FLearningAgentsActionSchemaElement& OutActionSchemaElement, ULearningAgentsActionSchema* InActionSchema)
{
    OutActionSchemaElement = ULearningAgentsActions::SpecifyContinuousAction(InActionSchema, 2, 1.0f, TetraAccelerationActionTag);
}

// 정책의 로컬 가속도 출력을 월드 평면 입력으로 변환해 물리 이동에 적용한다.
void UCMTetraLearningInteractor::PerformAgentAction_Implementation(const ULearningAgentsActionObject* InActionObject, const FLearningAgentsActionObjectElement& InActionObjectElement, int32 AgentId)
{
    AccelerationValuesScratch.Reset();
    if (!ULearningAgentsActions::GetContinuousAction(AccelerationValuesScratch, InActionObject, InActionObjectElement, TetraAccelerationActionTag) || AccelerationValuesScratch.Num() != 2)
        return;

    AActor* AgentActor = Cast<AActor>(GetAgent(AgentId));
    UCMAggressiveAccelerationMovementComponent* Movement = AgentActor ? AgentActor->FindComponentByClass<UCMAggressiveAccelerationMovementComponent>() : nullptr;
    ICMAggressiveMovementAgent* MovementAgent = Cast<ICMAggressiveMovementAgent>(AgentActor);
    UPrimitiveComponent* Body = MovementAgent ? MovementAgent->GetAggressiveMovementBody() : nullptr;
    if (!Movement || !Body)
        return;

    FVector LocalInput(AccelerationValuesScratch[0], AccelerationValuesScratch[1], 0.0f);
    LocalInput = LocalInput.GetClampedToMaxSize(1.0f);
    const FQuat BodyYaw(FVector::UpVector, FMath::DegreesToRadians(Body->GetComponentRotation().Yaw));
    Movement->SetAccelerationInput(BodyYaw.RotateVector(LocalInput));
}
