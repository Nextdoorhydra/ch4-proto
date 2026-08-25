#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Aggressive/Common/Core/CMAggressiveAITypes.h"

#include "CMCentipedeLearningInferenceCoordinator.generated.h"

class ACMCentipedePawn;
class ACMAggressiveChaseTestTarget;
class UCMCentipedeLearningInteractor;
class ULearningAgentsManager;
class ULearningAgentsPolicy;

UENUM(BlueprintType)
enum class ECMCentipedeMoveResult : uint8
{
    ReachedGoal,
    TimedOut,
    Failed,
    Cancelled
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCMCentipedeMoveCompletedSignature, ECMCentipedeMoveResult, Result);

/** 저장된 Centipede AI 정책으로 NavMesh 경로를 따라가는 독립 추론 실행기다. */
UCLASS(BlueprintType)
class AI_API ACMCentipedeLearningInferenceCoordinator : public AActor
{
    GENERATED_BODY()

public:
    ACMCentipedeLearningInferenceCoordinator();
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Centipede|Inference")
    bool StartInferencePath(ACMCentipedePawn* InInferenceAgent, FVector WorldGoal, float AcceptanceRadius = 120.0f);

    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Centipede|Inference|Chase Test")
    bool StartChasingTestTarget(ACMCentipedePawn* InInferenceAgent, ACMAggressiveChaseTestTarget* InChaseTarget, float AcceptanceRadius = 50.0f);

    UFUNCTION(BlueprintCallable, Category = "Aggressive AI|Centipede|Inference")
    void StopInference();

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Centipede|Inference")
    bool IsInferenceRunning() const { return bInferenceRunning; }

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Centipede|Inference")
    FString GetSnapshotDirectory() const;

    UPROPERTY(BlueprintAssignable, Category = "Aggressive AI|Centipede|Inference")
    FCMCentipedeMoveCompletedSignature OnCentipedeMoveCompleted;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Inference")
    TObjectPtr<ULearningAgentsManager> LearningManager;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Inference")
    TObjectPtr<ACMCentipedePawn> InferenceAgent;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Inference|Chase Test")
    TObjectPtr<ACMAggressiveChaseTestTarget> ChaseTarget;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Inference", meta = (ClampMin = "0.01"))
    float DecisionInterval = 0.08f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Inference", meta = (ClampMin = "1.0"))
    float MaximumInferenceSeconds = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Inference|Chase Test", meta = (ClampMin = "0.05"))
    float ChasePathRefreshInterval = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Inference|Chase Test", meta = (ClampMin = "1.0"))
    float ChaseRepathDistance = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Inference|Recovery", meta = (ClampMin = "0.1"))
    float ProgressCheckInterval = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Inference|Recovery", meta = (ClampMin = "0.1"))
    float MaximumNoProgressSeconds = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Inference|Recovery", meta = (ClampMin = "0.0"))
    float MinimumProgressDistance = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Inference|Recovery", meta = (ClampMin = "1"))
    int32 MaximumLateralEscapeAttempts = 2;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Inference|Recovery", meta = (ClampMin = "1.0"))
    float LateralEscapeProbeDistance = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Inference|Recovery", meta = (ClampMin = "1.0"))
    float LateralEscapeSpeed = 160.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Inference|Recovery", meta = (ClampMin = "0.1"))
    float LateralEscapeDuration = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Centipede|Inference|Recovery", meta = (ClampMin = "0.0"))
    float LateralEscapeMinimumDistance = 100.0f;

private:
    bool InitializeInferenceObjects();
    bool UpdateChaseTargetPath();
    bool BeginLateralEscape();
    bool UpdateLateralEscape();
    bool RebuildPathAfterLateralEscape();
    bool CanMoveBodyLaterally(const FVector& Direction) const;
    FVector GetBodyCenter() const;
    void ApplyLateralEscapeVelocity() const;
    void RunInferenceStep();
    void FinishInference(ECMCentipedeMoveResult Result, bool bBroadcast);

    UFUNCTION()
    void HandlePathMoveCompleted(ECMAggressivePathMoveResult Result);

    UPROPERTY(Transient)
    TObjectPtr<UCMCentipedeLearningInteractor> Interactor;

    UPROPERTY(Transient)
    TObjectPtr<ULearningAgentsPolicy> Policy;

    FTimerHandle InferenceTimerHandle;
    FVector ActiveWorldGoal = FVector::ZeroVector;
    FVector LateralEscapeStartLocation = FVector::ZeroVector;
    FVector LateralEscapeDirection = FVector::ZeroVector;
    float ActiveAcceptanceRadius = 120.0f;
    float LastGoalDistance = 0.0f;
    float NoProgressSeconds = 0.0f;
    double InferenceStartTime = 0.0;
    double NextChasePathRefreshTime = 0.0;
    double LastProgressCheckTime = 0.0;
    double LateralEscapeEndTime = 0.0;
    int32 InferenceAgentId = INDEX_NONE;
    int32 LateralEscapeAttemptCount = 0;
    bool bInferenceRunning = false;
    bool bChasingTestTarget = false;
    bool bWaitingForChaseTargetMove = false;
    bool bPerformingLateralEscape = false;
};
