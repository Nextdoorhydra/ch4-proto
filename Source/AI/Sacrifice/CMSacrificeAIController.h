#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Sacrifice/CMSacrificeAITypes.h"
#include "Sacrifice/CMSacrificeThreatTracker.h"

#include "CMSacrificeAIController.generated.h"

class ACMSacrificeCharacter;
struct FPathFollowingResult;

UENUM()
enum class ECMSacrificeBehaviorPhase : uint8
{
    Ambient,
    BackFall,
    BackCrawl,
    Escape,
    Vigilant,
    HitReact,
    GettingUp,
    Inactive
};

/** Server-only decision maker. Replicated Character/GAS state drives clients. */
UCLASS()
class AI_API ACMSacrificeAIController : public AAIController
{
    GENERATED_BODY()

public:
    ACMSacrificeAIController();
    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

private:
    UFUNCTION()
    void HandleAcceptedHit(AActor* Attacker, AActor* SourcePart, FVector ImpactDirection);

    UFUNCTION()
    void HandleSacrificeDied();

    void ScanForThreats();
    void RefreshFleeMovement();
    void UpdateForwardMovementFacing();
    bool RecoverToNavigation();
    void UpdateMovementProgress(float DeltaSeconds);
    void RecoverStalledMovement();
    void AcquireThreat(AActor* Threat, bool bFromHit);
    void StartThreatReaction();
    void StartBackCrawl();
    void RetryBackCrawlMovement();
    void StartEscapeLeg();
    void StartInjuredCrawl();
    void FinishHitReaction();
    void StartSafetyRecovery();
    void FinishSafetyInjuryFall();
    void StartGettingUp();
    void StartHitGettingUp(ECMSacrificeHitReactionDirection Direction);
    void FinishGettingUp();
    void StartVigilance();
    void FinishVigilance();
    void SelectAmbientAction();
    void ScheduleAmbientAction();
    bool MoveAwayFromThreat(float DistanceCm);
    bool MoveToProjectedLocation(const FVector& Goal, float AcceptanceRadius);
    void StopBehaviorTimers();
    void StopMovementForTransition();
    bool CannotAct() const;

    UPROPERTY(Transient)
    TObjectPtr<ACMSacrificeCharacter> Sacrifice;

    ECMSacrificeBehaviorPhase Phase = ECMSacrificeBehaviorPhase::Inactive;
    bool bThreatWasVisible = false;
    bool bFinishGoalAfterThreatLost = false;
    bool bIgnoreMoveCompletion = false;
    bool bResumeInjuredCrawlAfterHit = false;
    bool bResumeEscapeAfterGettingUp = false;
    float ThreatScanAccumulator = 0.0f;
    float StationaryMovementSeconds = 0.0f;
    FTimerHandle AmbientActionTimer;
    FTimerHandle BackFallTimer;
    FTimerHandle VigilanceTimer;
    FTimerHandle MoveRetryTimer;
    FTimerHandle HitReactionTimer;
    FTimerHandle GettingUpTimer;
    FVector LastFleeDirection = FVector::ZeroVector;
    FVector LastValidNavigationLocation = FVector::ZeroVector;
    FCMSacrificeThreatTracker ThreatTracker;
    bool bHasLastValidNavigationLocation = false;

    static constexpr float FleeDirectionRefreshDegrees = 1.0f;
    static constexpr float MinimumMovingSpeedCmPerSecond = 5.0f;
    static constexpr float MinimumMovementGoalDistanceCm = 100.0f;
    static constexpr float MovementStallTimeoutSeconds = 0.75f;
    static constexpr float NavigationContainmentToleranceCm = 25.0f;
};
