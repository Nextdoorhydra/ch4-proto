#pragma once

#include "CoreMinimal.h"
#include "LearningAgentsInteractor.h"

#include "CMCentipedeLearningInteractor.generated.h"

class ULearningAgentsManager;

/** Centipede AI의 머리 이동, 세 관절 상태, 네 마디 속도와 여덟 다리 행동을 연결한다. */
UCLASS(BlueprintType)
class AI_API UCMCentipedeLearningInteractor : public ULearningAgentsInteractor
{
    GENERATED_BODY()

public:
    static UCMCentipedeLearningInteractor* MakeCentipedeInteractor(ULearningAgentsManager*& InManager, FName Name = TEXT("CentipedeInteractor"));

    virtual void SpecifyAgentObservation_Implementation(FLearningAgentsObservationSchemaElement& OutObservationSchemaElement, ULearningAgentsObservationSchema* InObservationSchema) override;
    virtual void GatherAgentObservation_Implementation(FLearningAgentsObservationObjectElement& OutObservationObjectElement, ULearningAgentsObservationObject* InObservationObject, int32 AgentId) override;
    virtual void SpecifyAgentAction_Implementation(FLearningAgentsActionSchemaElement& OutActionSchemaElement, ULearningAgentsActionSchema* InActionSchema) override;
    virtual void PerformAgentAction_Implementation(const ULearningAgentsActionObject* InActionObject, const FLearningAgentsActionObjectElement& InActionObjectElement, int32 AgentId) override;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Centipede|Learning")
    int32 GetConfiguredLegCount() const { return 8; }

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Centipede|Learning")
    int32 GetConfiguredJointCount() const { return 3; }

private:
    TArray<int32> ActiveLegIndicesScratch;
};
