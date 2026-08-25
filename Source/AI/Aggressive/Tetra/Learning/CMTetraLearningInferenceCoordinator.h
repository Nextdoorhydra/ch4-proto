#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CMTetraLearningInferenceCoordinator.generated.h"

class ACMTetraPawn;
class ACMAggressiveChaseTestTarget;
class APawn;
class UCMTetraLearningInteractor;
class ULearningAgentsManager;
class ULearningAgentsPolicy;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class ECMTetraInterferenceState : uint8
{
    Inactive,
    Chasing,
    BackingUp,
    Dashing,
    Resting,
    RecoveringFromStuck
};

/** Tetra의 학습 추격과 코드 제어 후진·돌진·휴지를 실행한다. */
UCLASS(BlueprintType)
class AI_API ACMTetraLearningInferenceCoordinator : public AActor
{
    GENERATED_BODY()

public:
    ACMTetraLearningInferenceCoordinator();

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Tetra|Inference")
    bool StartInterference(ACMTetraPawn* InInferenceAgent, APawn* InTargetPlayer);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Tetra|Inference")
    bool StartInterferenceWithNearestPlayer(ACMTetraPawn* InInferenceAgent);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Tetra|Inference|Chase Test")
    bool StartChasingTestTarget(ACMTetraPawn* InInferenceAgent, ACMAggressiveChaseTestTarget* InChaseTarget, float AcceptanceRadius = 50.0f);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Tetra|Inference")
    void StopInterference();

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Tetra|Inference")
    bool IsInterferenceRunning() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Tetra|Inference")
    ECMTetraInterferenceState GetInterferenceState() const;

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Tetra|Inference")
    FString GetSnapshotDirectory() const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Inference")
    TObjectPtr<ULearningAgentsManager> LearningManager;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Inference")
    TObjectPtr<ACMTetraPawn> InferenceAgent;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Inference")
    TObjectPtr<APawn> TargetPlayer;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Inference|Chase Test")
    TObjectPtr<ACMAggressiveChaseTestTarget> ChaseTarget;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Inference", meta = (ClampMin = "0.01"))
    float DecisionInterval = 0.08f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Interference", meta = (ClampMin = "0.0"))
    float ApproachDistance = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Interference", meta = (ClampMin = "0.0"))
    float BackUpDistance = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Interference", meta = (ClampMin = "0.0"))
    float BackUpSpeed = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Interference", meta = (ClampMin = "0.1"))
    float MaximumBackUpSeconds = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Interference", meta = (ClampMin = "0.0"))
    float DashSpeed = 1200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Interference", meta = (ClampMin = "0.1"))
    float MaximumDashSeconds = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Interference", meta = (ClampMin = "0.0"))
    float PlayerKnockbackSpeed = 900.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Interference", meta = (ClampMin = "0.0"))
    float RestSeconds = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Inference", meta = (ClampMin = "1.0"))
    float ChaseRepathDistance = 75.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Inference|Stuck Recovery", meta = (ClampMin = "0.1"))
    float StuckDetectionSeconds = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggressive AI|Tetra|Inference|Stuck Recovery", meta = (ClampMin = "0.0"))
    float StuckMovementDistance = 15.0f;

private:
    bool InitializeInferenceObjects();
    bool StartTargetTracking(ACMTetraPawn* InInferenceAgent);
    bool BeginChase();
    void BeginBackUp();
    void BeginStuckRecovery();
    void BeginCodeControlledBackUp(FVector Direction, ECMTetraInterferenceState BackUpState);
    void BeginDash();
    void BeginRest();
    void RunInterferenceStep();
    void UpdateChase();
    void UpdateBackUp();
    void UpdateStuckRecovery();
    void UpdateDash();
    void UpdateRest();
    bool HasBackUpFinished() const;
    void ResetChaseProgress();
    void SetCodePlanarVelocity(FVector Direction, float Speed);
    void ApplyKnockbackToTarget();
    AActor* GetActiveTarget() const;
    APawn* FindNearestPlayerPawn(const ACMTetraPawn* ReferenceAgent) const;
    UFUNCTION()
    void HandleAgentHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

    UPROPERTY(Transient)
    TObjectPtr<UCMTetraLearningInteractor> Interactor;

    UPROPERTY(Transient)
    TObjectPtr<ULearningAgentsPolicy> Policy;

    ECMTetraInterferenceState State = ECMTetraInterferenceState::Inactive;
    int32 InferenceAgentId = INDEX_NONE;
    FVector LastChaseGoal = FVector::ZeroVector;
    FVector ChaseProgressLocation = FVector::ZeroVector;
    FVector BackUpStartLocation = FVector::ZeroVector;
    FVector BackUpDirection = FVector::ZeroVector;
    FVector DashDirection = FVector::ZeroVector;
    double StateStartTime = 0.0;
    double ChaseProgressStartTime = 0.0;
    float TestTargetAcceptanceRadius = 50.0f;
    bool bChasingTestTarget = false;
    FTimerHandle InterferenceTimerHandle;
};
