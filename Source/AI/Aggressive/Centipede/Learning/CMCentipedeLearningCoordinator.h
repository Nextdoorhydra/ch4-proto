#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Aggressive/Common/Learning/CMAggressiveLearningTrainingEnvironment.h"

#include "CMCentipedeLearningCoordinator.generated.h"

class ACMCentipedePawn;
class UCMCentipedeLearningInteractor;
class UCMCentipedeLearningTrainingEnvironment;
class ULearningAgentsCritic;
class ULearningAgentsManager;
class ULearningAgentsPolicy;
class ULearningAgentsPPOTrainer;

/** Centipede AI의 독립 PPO 정책을 생성, 실행, 저장한다. */
UCLASS(BlueprintType)
class AI_API ACMCentipedeLearningCoordinator : public AActor
{
    GENERATED_BODY()

public:
    ACMCentipedeLearningCoordinator();
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Centipede|Learning")
    bool StartTraining(ACMCentipedePawn* InTrainingAgent);

    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Centipede|Learning")
    bool StartTrainingAgents(const TArray<ACMCentipedePawn*>& InTrainingAgents);

    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Centipede|Learning")
    bool StartTrainingAllAgents();

    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Centipede|Learning")
    void StopTraining();

    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Centipede|Learning")
    bool SaveTrainingSnapshots();

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Centipede|Learning")
    bool IsTraining() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Centipede|Learning")
    FString GetSnapshotDirectory() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Centipede|Learning")
    int32 GetTrainingAgentCount() const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Learning")
    TObjectPtr<ULearningAgentsManager> LearningManager;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Learning")
    TArray<TObjectPtr<ACMCentipedePawn>> TrainingAgents;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Learning", meta = (ClampMin = "0.01"))
    float DecisionInterval = 0.08f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Learning")
    FCMAggressiveLearningRewardSettings RewardSettings;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Learning")
    FCMAggressiveLearningGoalSettings GoalSettings;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Learning", meta = (ClampMin = "0.0"))
    float JointTrackingPenaltyScale = 0.04f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Learning", meta = (ClampMin = "100"))
    int32 MaximumRecordedStepsPerIteration = 4000;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Learning", meta = (ClampMin = "1.0"))
    float SnapshotSaveIntervalSeconds = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Learning", meta = (ClampMin = "0.1"))
    float ProgressLogIntervalSeconds = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Learning")
    bool bResumeExistingSnapshots = true;

private:
    bool InitializeLearningObjects();
    void RunTrainingStep();
    void LogTrainingProgressIfNeeded();
    void RefreshPolicyUpdateState();

    UPROPERTY(Transient)
    TObjectPtr<UCMCentipedeLearningInteractor> Interactor;

    UPROPERTY(Transient)
    TObjectPtr<UCMCentipedeLearningTrainingEnvironment> TrainingEnvironment;

    UPROPERTY(Transient)
    TObjectPtr<ULearningAgentsPolicy> Policy;

    UPROPERTY(Transient)
    TObjectPtr<ULearningAgentsCritic> Critic;

    UPROPERTY(Transient)
    TObjectPtr<ULearningAgentsPPOTrainer> PPOTrainer;

    TArray<int32> TrainingAgentIds;
    FTimerHandle TrainingTimerHandle;
    int32 InitialPolicyContentHash = 0;
    int64 TotalTrainingStepCount = 0;
    int64 TotalAgentDecisionCount = 0;
    double NextProgressLogTime = 0.0;
    double NextSnapshotSaveTime = 0.0;
    bool bHasReceivedPolicyUpdate = false;
};
