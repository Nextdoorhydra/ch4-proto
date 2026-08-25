#pragma once

#include "CoreMinimal.h"
#include "LearningAgentsInteractor.h"

#include "CMAggressiveLearningInteractor.generated.h"

class ULearningAgentsManager;

/**
 * 공격적 AI의 이동 관측과 다리 행동을 Learning Agents에 연결한다.
 *
 * 하나의 Interactor는 같은 몸통 구성의 AI만 관리한다. 몸통의 다리 수가
 * 바뀌면 해당 수에 맞는 새 Interactor와 정책을 만들어야 한다.
 */
UCLASS(BlueprintType)
class AI_API UCMAggressiveLearningInteractor
    : public ULearningAgentsInteractor
{
    GENERATED_BODY()

public:
    /** 지정한 다리 수로 스키마를 확정하고 Interactor를 생성한다. */
    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Learning")
    static UCMAggressiveLearningInteractor* MakeAggressiveInteractor(UPARAM(ref) ULearningAgentsManager*& InManager, int32 InLegCount, FName Name = TEXT("AggressiveMovementInteractor"));

    virtual void SpecifyAgentObservation_Implementation(FLearningAgentsObservationSchemaElement& OutObservationSchemaElement, ULearningAgentsObservationSchema* InObservationSchema) override;

    virtual void GatherAgentObservation_Implementation(FLearningAgentsObservationObjectElement& OutObservationObjectElement, ULearningAgentsObservationObject* InObservationObject, int32 AgentId) override;

    virtual void SpecifyAgentAction_Implementation(FLearningAgentsActionSchemaElement& OutActionSchemaElement, ULearningAgentsActionSchema* InActionSchema) override;

    virtual void PerformAgentAction_Implementation(const ULearningAgentsActionObject* InActionObject, const FLearningAgentsActionObjectElement& InActionObjectElement, int32 AgentId) override;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Learning")
    int32 GetConfiguredLegCount() const;

private:
    UPROPERTY(VisibleAnywhere, Category = "Aggressive AI|Learning")
    int32 ConfiguredLegCount = 0;

    TArray<int32> ActiveLegIndicesScratch;
    TArray<float> LegActivationSignalsScratch;
};
