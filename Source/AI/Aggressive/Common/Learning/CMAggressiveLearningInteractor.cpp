#include "Aggressive/Common/Learning/CMAggressiveLearningInteractor.h"

#include "Aggressive/Common/Core/CMAggressiveMovementAgent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "LearningAgentsActions.h"
#include "LearningAgentsManager.h"
#include "LearningAgentsObservations.h"
#include "Aggressive/Common/Movement/CMAggressiveMovementCommandComponent.h"

namespace
{
    const FName GoalDirectionTag(TEXT("GoalDirection"));
    const FName PlanarVelocityTag(TEXT("PlanarVelocity"));
    const FName YawAngularVelocityTag(TEXT("YawAngularVelocity"));
    const FName MovementObservationTag(TEXT("MovementObservation"));
    const FName LegActivationsTag(TEXT("LegActivations"));

    constexpr float PlanarVelocityScale = 600.0f;
    constexpr float YawAngularVelocityScale = 10.0f;
}

// 지정한 다리 수로 이동 관측과 행동 스키마를 가진 Interactor를 생성한다.
UCMAggressiveLearningInteractor* UCMAggressiveLearningInteractor::MakeAggressiveInteractor(ULearningAgentsManager*& InManager, int32 InLegCount, FName Name)
{
    if (!InManager || InLegCount <= 0)
        return nullptr;

    const FName UniqueName = MakeUniqueObjectName(InManager, StaticClass(), Name, EUniqueObjectNameOptions::GloballyUnique);
    UCMAggressiveLearningInteractor* Interactor = NewObject<UCMAggressiveLearningInteractor>(InManager, StaticClass(), UniqueName);
    if (!Interactor)
        return nullptr;

    Interactor->ConfiguredLegCount = InLegCount;
    Interactor->ActiveLegIndicesScratch.Reserve(InLegCount);
    Interactor->LegActivationSignalsScratch.Reserve(InLegCount);
    Interactor->SetupInteractor(InManager);
    return Interactor->IsSetup() ? Interactor : nullptr;
}

// 목표 방향과 몸통 속도로 구성된 이동 관측 스키마를 정의한다.
void UCMAggressiveLearningInteractor::SpecifyAgentObservation_Implementation(FLearningAgentsObservationSchemaElement& OutObservationSchemaElement, ULearningAgentsObservationSchema* InObservationSchema)
{
    const FName ElementNames[] = {GoalDirectionTag, PlanarVelocityTag, YawAngularVelocityTag};
    const FLearningAgentsObservationSchemaElement Elements[] = {
        ULearningAgentsObservations::SpecifyEnumObservation(InObservationSchema, StaticEnum<ECMAggressiveMoveDirection>(), GoalDirectionTag),
        ULearningAgentsObservations::SpecifyContinuousObservation(InObservationSchema, 2, PlanarVelocityScale, PlanarVelocityTag),
        ULearningAgentsObservations::SpecifyFloatObservation(InObservationSchema, YawAngularVelocityScale, YawAngularVelocityTag)
    };

    OutObservationSchemaElement = ULearningAgentsObservations::SpecifyStructObservationFromArrayViews(InObservationSchema, MakeArrayView(ElementNames), MakeArrayView(Elements), MovementObservationTag);
}

// 지정한 AI Pawn의 현재 이동 방향과 물리 속도를 관측값으로 수집한다.
void UCMAggressiveLearningInteractor::GatherAgentObservation_Implementation(FLearningAgentsObservationObjectElement& OutObservationObjectElement, ULearningAgentsObservationObject* InObservationObject, int32 AgentId)
{
    ECMAggressiveMoveDirection GoalDirection = ECMAggressiveMoveDirection::None;
    FVector LocalVelocity = FVector::ZeroVector;
    float YawAngularVelocity = 0.0f;

    AActor* AgentActor = Cast<AActor>(GetAgent(AgentId));
    ICMAggressiveMovementAgent* MovementAgent = Cast<ICMAggressiveMovementAgent>(AgentActor);
    UPrimitiveComponent* Body = MovementAgent ? MovementAgent->GetAggressiveMovementBody() : nullptr;
    UCMAggressiveMovementCommandComponent* MovementCommand = AgentActor ? AgentActor->FindComponentByClass<UCMAggressiveMovementCommandComponent>() : nullptr;
    if (Body && MovementCommand)
    {
        GoalDirection = MovementCommand->RefreshGoalDirection();
        const float BodyYawRadians = FMath::DegreesToRadians(Body->GetComponentRotation().Yaw);
        const FQuat BodyYawRotation(FVector::UpVector, BodyYawRadians);
        LocalVelocity = BodyYawRotation.UnrotateVector(Body->GetPhysicsLinearVelocity());
        YawAngularVelocity = Body->GetPhysicsAngularVelocityInRadians().Z;
    }

    const float PlanarVelocity[] = {static_cast<float>(LocalVelocity.X), static_cast<float>(LocalVelocity.Y)};
    const FName ElementNames[] = {GoalDirectionTag, PlanarVelocityTag, YawAngularVelocityTag};
    const FLearningAgentsObservationObjectElement Elements[] = {
        ULearningAgentsObservations::MakeEnumObservation(InObservationObject, StaticEnum<ECMAggressiveMoveDirection>(), static_cast<uint8>(GoalDirection), GoalDirectionTag),
        ULearningAgentsObservations::MakeContinuousObservationFromArrayView(InObservationObject, MakeArrayView(PlanarVelocity), PlanarVelocityTag),
        ULearningAgentsObservations::MakeFloatObservation(InObservationObject, YawAngularVelocity, YawAngularVelocityTag)
    };

    OutObservationObjectElement = ULearningAgentsObservations::MakeStructObservationFromArrayViews(InObservationObject, MakeArrayView(ElementNames), MakeArrayView(Elements), MovementObservationTag);
}

// 각 다리를 독립적으로 선택하는 행동 스키마를 정의한다.
void UCMAggressiveLearningInteractor::SpecifyAgentAction_Implementation(FLearningAgentsActionSchemaElement& OutActionSchemaElement, ULearningAgentsActionSchema* InActionSchema)
{
    OutActionSchemaElement = ULearningAgentsActions::SpecifyInclusiveDiscreteAction(InActionSchema, ConfiguredLegCount, {}, LegActivationsTag);
}

// 정책이 선택한 다리 행동을 지정한 AI Pawn에 적용한다.
void UCMAggressiveLearningInteractor::PerformAgentAction_Implementation(const ULearningAgentsActionObject* InActionObject, const FLearningAgentsActionObjectElement& InActionObjectElement, int32 AgentId)
{
    ActiveLegIndicesScratch.Reset();
    if (!ULearningAgentsActions::GetInclusiveDiscreteAction(ActiveLegIndicesScratch, InActionObject, InActionObjectElement, LegActivationsTag))
        return;

    AActor* AgentActor = Cast<AActor>(GetAgent(AgentId));
    UCMAggressiveMovementCommandComponent* MovementCommand = AgentActor ? AgentActor->FindComponentByClass<UCMAggressiveMovementCommandComponent>() : nullptr;
    if (!MovementCommand)
        return;

    LegActivationSignalsScratch.Init(0.0f, ConfiguredLegCount);
    for (const int32 LegIndex : ActiveLegIndicesScratch)
    {
        if (LegActivationSignalsScratch.IsValidIndex(LegIndex))
            LegActivationSignalsScratch[LegIndex] = 1.0f;
    }
    MovementCommand->ApplyLegActivationSignals(LegActivationSignalsScratch);
}

// 이 Interactor에 고정된 다리 수를 반환한다.
int32 UCMAggressiveLearningInteractor::GetConfiguredLegCount() const
{
    return ConfiguredLegCount;
}
