#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Aggressive/Common/Learning/CMAggressiveLearningTrainingEnvironment.h"

#include "CMRipperLearningCoordinator.generated.h"

class ACMRipperPawn;
class UCMAggressiveLearningInteractor;
class ULearningAgentsCritic;
class ULearningAgentsManager;
class ULearningAgentsPolicy;
class ULearningAgentsPPOTrainer;

/** Ripper AI의 세 다리 임펄스 PPO 학습을 타이머로 관리한다. */
UCLASS(BlueprintType)
class AI_API ACMRipperLearningCoordinator : public AActor
{
    GENERATED_BODY()

public:
    ACMRipperLearningCoordinator();

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Ripper|Learning")
    bool StartTraining(ACMRipperPawn* InTrainingAgent);

    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Ripper|Learning")
    bool StartTrainingAgents(const TArray<ACMRipperPawn*>& InTrainingAgents);

    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Ripper|Learning")
    bool StartTrainingAllAgents();

    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Ripper|Learning")
    void StopTraining();

    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Ripper|Learning")
    bool SaveTrainingSnapshots();

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Ripper|Learning")
    bool IsTraining() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Ripper|Learning")
    FString GetSnapshotDirectory() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Ripper|Learning")
    int32 GetTrainingAgentCount() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Ripper|Learning")
    bool HasReceivedPolicyUpdate() const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Learning")
    TObjectPtr<ULearningAgentsManager> LearningManager;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Learning")
    TArray<TObjectPtr<ACMRipperPawn>> TrainingAgents;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Learning", meta = (ClampMin = "0.01"))
    float DecisionInterval = 0.08f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Learning")
    FCMAggressiveLearningRewardSettings RewardSettings;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Learning")
    FCMAggressiveLearningGoalSettings GoalSettings;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Learning", meta = (ClampMin = "1.0"))
    float SnapshotSaveIntervalSeconds = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Learning", meta = (ClampMin = "100"))
    int32 MaximumRecordedStepsPerIteration = 2400;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Learning")
    bool bResumeExistingSnapshots = true;

private:
    bool InitializeLearningObjects();
    void RefreshPolicyUpdateState();
    void RunTrainingStep();
    void SaveTrainingSnapshotsIfNeeded();

    UPROPERTY(Transient)
    TObjectPtr<UCMAggressiveLearningInteractor> Interactor;

    UPROPERTY(Transient)
    TObjectPtr<UCMAggressiveLearningTrainingEnvironment> TrainingEnvironment;

    UPROPERTY(Transient)
    TObjectPtr<ULearningAgentsPolicy> Policy;

    UPROPERTY(Transient)
    TObjectPtr<ULearningAgentsCritic> Critic;

    UPROPERTY(Transient)
    TObjectPtr<ULearningAgentsPPOTrainer> PPOTrainer;

    TArray<int32> TrainingAgentIds;
    FTimerHandle TrainingTimerHandle;
    int32 InitialPolicyContentHash = 0;
    double NextSnapshotSaveTime = 0.0;
    bool bHasReceivedPolicyUpdate = false;
};
