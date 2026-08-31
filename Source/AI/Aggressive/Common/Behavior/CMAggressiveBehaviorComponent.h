#pragma once

#include "CoreMinimal.h"
#include "Aggressive/Common/Core/CMAggressiveAITypes.h"
#include "Components/ActorComponent.h"

#include "CMAggressiveBehaviorComponent.generated.h"

class ACMAggressivePawnBase;
class ACMCentipedeLearningInferenceCoordinator;
class ACMChimera;
class ACMRipperLearningInferenceCoordinator;
class ACMSacrificeCharacter;
class ACMTetraLearningInferenceCoordinator;
class ICMAggressiveMovementAgent;
class UCMAggressiveSightComponent;
class UPrimitiveComponent;

UENUM()
enum class ECMAggressiveBehaviorProfile : uint8
{
    Tetra,
    Ripper,
    Centipede
};

/** Server-authoritative high-level behavior shared by the three hostile AI. */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class AI_API UCMAggressiveBehaviorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMAggressiveBehaviorComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    void ConfigureProfile(ECMAggressiveBehaviorProfile InProfile);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Aggressive AI|Behavior")
    void SetBehaviorEnabled(bool bEnabled);

    UFUNCTION(BlueprintPure, Category = "Aggressive AI|Behavior")
    ECMAggressiveAIState GetBehaviorState() const
    {
        return State;
    }

private:
    void InitializeRuntimeBehavior();
    void UpdateBehavior();
    void UpdateSearching(double CurrentTime);
    void UpdateChasing();
    void UpdateAttacking(double CurrentTime);
    void UpdateWaiting(double CurrentTime);
    void UpdateTetraSightScan(double CurrentTime);
    bool UpdateStuckRecovery(double CurrentTime);
    bool UpdateStuckDetection(double CurrentTime);
    void BeginStuckRecovery(double CurrentTime);
    void ApplyStuckRecoveryReverseVelocity() const;
    void ResetStuckTracking();
    void BeginChasing(AActor* NewTarget);
    void BeginReturningHome();
    void BeginWaiting(float Seconds);
    bool BeginRandomMove();
    void DrawWanderGoalDebug() const;
    bool StartMove(FVector Goal, float AcceptanceRadius);
    bool UpdateMoveGoal(FVector Goal);
    void StopMove();
    bool IsMoveRunning() const;
    float GetAttackDistance() const;
    ICMAggressiveMovementAgent* GetMovementAgent() const;
    FVector GetNavigationLocation() const;
    FVector GetAttackOriginLocation() const;
    UPrimitiveComponent* GetMovementBody() const;
    AActor* FindVisibleTarget() const;
    ACMSacrificeCharacter* FindCloserVisibleSacrifice(const ACMSacrificeCharacter& CurrentSacrifice) const;
    bool CanSeeActor(const AActor& Target) const;
    bool IsValidTarget(const AActor* Target) const;
    bool PerformRipperAttack(AActor& Target);
    bool PerformCentipedeAttack(AActor& Target);
    void BeginTetraDash();
    void FinishTetraDashWithoutHit();
    void HandleTetraKnockbackFinished();
    void DestroyInferenceCoordinator();

    UFUNCTION()
    void HandleOwnerHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);

    UPROPERTY(EditAnywhere, Category = "Aggressive AI|Behavior")
    bool bAutoStartBehavior = true;

    UPROPERTY(EditAnywhere, Category = "Aggressive AI|Behavior|Attack", meta = (ClampMin = "0.0", ForceUnits = "cm"))
    float RipperAttackDistance = 200.0f;

    UPROPERTY(EditAnywhere, Category = "Aggressive AI|Behavior|Attack", meta = (ClampMin = "0.0", ForceUnits = "cm"))
    float CentipedeAttackDistance = 130.0f;

    UPROPERTY(VisibleInstanceOnly, Category = "Aggressive AI|Behavior")
    ECMAggressiveBehaviorProfile Profile = ECMAggressiveBehaviorProfile::Tetra;

    UPROPERTY(VisibleInstanceOnly, Category = "Aggressive AI|Behavior")
    ECMAggressiveAIState State = ECMAggressiveAIState::Searching;

    UPROPERTY(Transient)
    TObjectPtr<ACMAggressivePawnBase> OwnerPawn;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UCMAggressiveSightComponent>> SightComponents;

    UPROPERTY(Transient)
    TObjectPtr<AActor> CurrentTarget;

    UPROPERTY(Transient)
    TObjectPtr<ACMTetraLearningInferenceCoordinator> TetraCoordinator;

    UPROPERTY(Transient)
    TObjectPtr<ACMRipperLearningInferenceCoordinator> RipperCoordinator;

    UPROPERTY(Transient)
    TObjectPtr<ACMCentipedeLearningInferenceCoordinator> CentipedeCoordinator;

    FVector SpawnLocation = FVector::ZeroVector;
    FVector LastMoveGoal = FVector::ZeroVector;
    UPROPERTY(Replicated)
    FVector LastWanderGoal = FVector::ZeroVector;

    FVector DashDirection = FVector::ForwardVector;
    FVector StuckProgressLocation = FVector::ZeroVector;
    FVector StuckReverseDirection = FVector::ZeroVector;
    double NextActionTime = 0.0;
    double DashEndTime = 0.0;
    double NextTetraSightTurnTime = 0.0;
    double StuckProgressStartTime = 0.0;
    double StuckReverseEndTime = 0.0;
    bool bBehaviorEnabled = false;
    bool bMoveIssued = false;
    bool bReturningHome = false;
    bool bReversingFromStuck = false;
    bool bCompletingStuckRecoveryMove = false;
    UPROPERTY(Replicated)
    bool bHasWanderGoal = false;

    FTimerHandle BehaviorTimerHandle;
    ICMAggressiveMovementAgent* CachedMovementAgent = nullptr;
};
