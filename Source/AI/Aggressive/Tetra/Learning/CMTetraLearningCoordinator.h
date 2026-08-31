#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Aggressive/Common/Learning/CMAggressiveLearningTrainingEnvironment.h"

#include "CMTetraLearningCoordinator.generated.h"

class ACMTetraPawn;
class UCMTetraLearningInteractor;
class ULearningAgentsCritic;
class ULearningAgentsManager;
class ULearningAgentsPolicy;
class ULearningAgentsPPOTrainer;

/** Tetra AI의 연속 가속 이동 정책을 병렬 PPO로 학습하고 체크포인트를 보존한다. */
UCLASS(BlueprintType)
class AI_API ACMTetraLearningCoordinator : public AActor
{
    GENERATED_BODY()

public:
    ACMTetraLearningCoordinator();

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Tetra|Learning")
    bool StartTraining(ACMTetraPawn* InTrainingAgent);

    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Tetra|Learning")
    bool StartTrainingAgents(const TArray<ACMTetraPawn*>& InTrainingAgents);

    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Tetra|Learning")
    bool StartTrainingAllAgents();

    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Tetra|Learning")
    void StopTraining();

    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Tetra|Learning")
    bool SaveTrainingSnapshots();

    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Tetra|Learning")
    bool SaveTrainingCheckpoint();

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Tetra|Learning")
    bool IsTraining() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Tetra|Learning")
    ULearningAgentsManager* GetLearningManager() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Tetra|Learning")
    FString GetSnapshotDirectory() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Tetra|Learning")
    FString GetCheckpointDirectory() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Tetra|Learning")
    int32 GetMaximumRecordedStepsPerIteration() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Tetra|Learning")
    int32 GetCheckpointDecisionInterval() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Tetra|Learning")
    int32 GetTrainingAgentCount() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Tetra|Learning")
    bool HasReceivedPolicyUpdate() const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Learning")
    TObjectPtr<ULearningAgentsManager> LearningManager;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Learning")
    TArray<TObjectPtr<ACMTetraPawn>> TrainingAgents;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Learning", meta = (ClampMin = "0.01"))
    float DecisionInterval = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Learning")
    FCMAggressiveLearningRewardSettings RewardSettings;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Learning")
    FCMAggressiveLearningGoalSettings GoalSettings;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Learning", meta = (ClampMin = "0.1"))
    float ProgressLogIntervalSeconds = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Learning", meta = (ClampMin = "1.0"))
    float SnapshotSaveIntervalSeconds = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Learning", meta = (ClampMin = "100"))
    int32 MaximumRecordedStepsPerIteration = 256;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Learning", meta = (ClampMin = "100"))
    int32 CheckpointDecisionInterval = 1000;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Learning")
    bool bResumeExistingSnapshots = true;

private:
    bool InitializeLearningObjects();
    void RefreshPolicyUpdateState();
    void RunTrainingStep();
    void LogTrainingProgressIfNeeded();
    void SaveTrainingSnapshotsIfNeeded();
    void SaveTrainingCheckpointIfNeeded();

    UPROPERTY(Transient)
    TObjectPtr<UCMTetraLearningInteractor> Interactor;

    UPROPERTY(Transient)
    TObjectPtr<ULearningAgentsPolicy> Policy;

    UPROPERTY(Transient)
    TObjectPtr<UCMAggressiveLearningTrainingEnvironment> TrainingEnvironment;

    UPROPERTY(Transient)
    TObjectPtr<ULearningAgentsCritic> Critic;

    UPROPERTY(Transient)
    TObjectPtr<ULearningAgentsPPOTrainer> PPOTrainer;

    TArray<int32> TrainingAgentIds;
    FTimerHandle TrainingTimerHandle;
    int64 TotalAgentDecisionCount = 0;
    int64 NextCheckpointDecisionCount = 0;
    int32 InitialPolicyContentHash = 0;
    int32 CurrentPolicyContentHash = 0;
    int32 LastCheckpointPolicyContentHash = 0;
    double NextProgressLogTime = 0.0;
    double NextSnapshotSaveTime = 0.0;
    bool bHasReceivedPolicyUpdate = false;
};
