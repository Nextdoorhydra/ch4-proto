#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Aggressive/Common/Core/CMAggressiveAITypes.h"

#include "CMRipperLearningInferenceCoordinator.generated.h"

class ACMRipperPawn;
class ACMAggressiveChaseTestTarget;
class UCMAggressiveLearningInteractor;
class ULearningAgentsManager;
class ULearningAgentsPolicy;

/** Ripper AI 저장 정책의 목적지 이동 결과다. */
UENUM(BlueprintType)
enum class ECMRipperMoveResult : uint8
{
    ReachedGoal,
    TimedOut,
    Failed,
    Cancelled
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCMRipperMoveCompletedSignature, ECMRipperMoveResult, Result);

/** Ripper AI의 저장 정책과 NavMesh 경로 입력을 타이머로 실행한다. */
UCLASS(BlueprintType)
class AI_API ACMRipperLearningInferenceCoordinator : public AActor
{
    GENERATED_BODY()

public:
    ACMRipperLearningInferenceCoordinator();

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Ripper|Inference")
    bool StartInferencePath(ACMRipperPawn* InInferenceAgent, FVector WorldGoal, float AcceptanceRadius = 50.0f);

    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Ripper|Inference")
    bool UpdateInferenceGoal(FVector WorldGoal);

    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Ripper|Inference|Chase Test")
    bool StartChasingTestTarget(ACMRipperPawn* InInferenceAgent, ACMAggressiveChaseTestTarget* InChaseTarget, float AcceptanceRadius = 50.0f);

    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Ripper|Inference")
    void StopInference();

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Ripper|Inference")
    bool IsInferenceRunning() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Ripper|Inference")
    FString GetSnapshotDirectory() const;

    UPROPERTY(BlueprintAssignable, Category = "Aggressive AI|Ripper|Inference")
    FCMRipperMoveCompletedSignature OnRipperMoveCompleted;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Inference")
    TObjectPtr<ULearningAgentsManager> LearningManager;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Inference")
    TObjectPtr<ACMRipperPawn> InferenceAgent;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Inference|Chase Test")
    TObjectPtr<ACMAggressiveChaseTestTarget> ChaseTarget;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Inference", meta = (ClampMin = "0.01"))
    float DecisionInterval = 0.08f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Inference", meta = (ClampMin = "0.1"))
    float MaximumInferenceSeconds = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Inference|Chase Test", meta = (ClampMin = "0.05"))
    float ChasePathRefreshInterval = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Inference|Chase Test", meta = (ClampMin = "1.0"))
    float ChaseRepathDistance = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Inference|Recovery", meta = (ClampMin = "0.1"))
    float StuckCheckInterval = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Inference|Recovery", meta = (ClampMin = "0.0"))
    float StuckMinimumProgressDistance = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Inference|Recovery", meta = (ClampMin = "0.0"))
    float StuckMinimumAngularProgressDegrees = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Inference|Recovery", meta = (ClampMin = "0.1"))
    float StuckMaximumNoProgressSeconds = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Inference|Recovery", meta = (ClampMin = "0.1"))
    float StuckMaximumNoDistanceProgressSeconds = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Inference|Recovery", meta = (ClampMin = "1"))
    int32 MaximumStuckRecoveryAttempts = 3;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Inference|Recovery", meta = (ClampMin = "1.0"))
    float RecoveryWallSearchRadius = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Inference|Recovery", meta = (ClampMin = "1.0"))
    float RecoveryTargetDistance = 225.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Inference|Recovery", meta = (ClampMin = "1.0"))
    float RecoveryImpulseSpeed = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Inference|Recovery", meta = (ClampMin = "0.1"))
    float RecoveryDuration = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Ripper|Inference|Recovery", meta = (ClampMin = "0.0"))
    float RecoveryMinimumMoveDistance = 150.0f;

private:
    bool InitializeInferenceObjects();
    bool UpdateChaseTargetPath();
    bool UpdateStuckDetection();
    bool BeginStuckRecovery();
    bool UpdateStuckRecovery();
    bool RebuildPathAfterStuckRecovery();
    bool FindStuckRecoveryLocation(FVector& OutRecoveryLocation, FVector& OutRecoveryDirection) const;
    float CalculateCurrentGoalAngularError(FVector GoalLocation) const;
    void ApplyStuckRecoveryImpulse();
    void FinishInference(ECMRipperMoveResult Result, bool bBroadcastResult);
    void ResetStuckProgress();
    void StopBodyPlanarMotion() const;
    void RunInferenceStep();

    UFUNCTION()
    void HandlePathMoveCompleted(ECMAggressivePathMoveResult Result);

    UPROPERTY(Transient)
    TObjectPtr<UCMAggressiveLearningInteractor> Interactor;

    UPROPERTY(Transient)
    TObjectPtr<ULearningAgentsPolicy> Policy;

    FTimerHandle InferenceTimerHandle;
    int32 InferenceAgentId = INDEX_NONE;
    FVector ActiveWorldGoal = FVector::ZeroVector;
    FVector LastStuckPathPoint = FVector::ZeroVector;
    FVector RecoveryStartLocation = FVector::ZeroVector;
    FVector RecoverySequenceStartLocation = FVector::ZeroVector;
    FVector RecoveryTargetLocation = FVector::ZeroVector;
    FVector RecoveryDirection = FVector::ZeroVector;
    float ActiveAcceptanceRadius = 50.0f;
    float LastStuckGoalDistance = 0.0f;
    float LastStuckGoalAngularError = PI;
    float StuckNoProgressSeconds = 0.0f;
    float StuckNoDistanceProgressSeconds = 0.0f;
    double InferenceStartTime = 0.0;
    double NextChasePathRefreshTime = 0.0;
    double LastStuckCheckTime = 0.0;
    double RecoveryEndTime = 0.0;
    int32 StuckRecoveryAttemptCount = 0;
    bool bInferenceRunning = false;
    bool bRecoveringFromStuck = false;
    bool bChasingTestTarget = false;
    bool bWaitingForChaseTargetMove = false;
};
