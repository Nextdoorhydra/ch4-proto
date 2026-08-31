#pragma once

#include "CoreMinimal.h"
#include "LearningAgentsInteractor.h"

#include "CMTetraLearningInteractor.generated.h"

class ULearningAgentsManager;
struct FLearningAgentsPolicySettings;

/** Tetra AI의 목표·속도 관측을 연속 평면 가속도 행동에 연결한다. */
UCLASS(BlueprintType)
class AI_API UCMTetraLearningInteractor : public ULearningAgentsInteractor
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Tetra|Learning")
    static UCMTetraLearningInteractor* MakeTetraInteractor(UPARAM(ref) ULearningAgentsManager*& InManager, FName Name = TEXT("TetraInteractor"));

    static FLearningAgentsPolicySettings GetPolicySettings();

    virtual void SpecifyAgentObservation_Implementation(FLearningAgentsObservationSchemaElement& OutObservationSchemaElement, ULearningAgentsObservationSchema* InObservationSchema) override;
    virtual void GatherAgentObservation_Implementation(FLearningAgentsObservationObjectElement& OutObservationObjectElement, ULearningAgentsObservationObject* InObservationObject, int32 AgentId) override;
    virtual void SpecifyAgentAction_Implementation(FLearningAgentsActionSchemaElement& OutActionSchemaElement, ULearningAgentsActionSchema* InActionSchema) override;
    virtual void PerformAgentAction_Implementation(const ULearningAgentsActionObject* InActionObject, const FLearningAgentsActionObjectElement& InActionObjectElement, int32 AgentId) override;

private:
    TArray<float> AccelerationValuesScratch;
};
