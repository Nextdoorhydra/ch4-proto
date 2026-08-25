#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CMLineBodyMovementCoordinator.generated.h"

class ACMChimera;
class ACMLegPart;
class ACMPlayerState;
class AActor;
class USceneComponent;
class UStaticMeshComponent;

/**
 * Owns the LineBody movement calculation flow.
 *
 * This component does not Tick. ACMChimera forwards only the existing server
 * physics update and successful input events, while this object keeps the
 * ground test, impulse math, cooperative-input window, and speed cap together.
 */
UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
class CHIMERA_API UCMLineBodyMovementCoordinator
    : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMLineBodyMovementCoordinator();

    /** Starts one server-authoritative grounded push for the specified Leg. */
    bool TryActivateLeg(
        ACMChimera& Chimera,
        ACMLegPart& LegPart,
        ACMPlayerState* ContributingPlayerState,
        float MovementImpulseMultiplier
    );

    /** Stops only the active push owned by this Leg, if one exists. */
    void CancelLegStep(const ACMLegPart* LegPart);

    /** Applies an immediate, non-grounded impulse from an Arm slot. */
    bool TryActivateArm(
        ACMChimera& Chimera,
        int32 SegmentIndex,
        USceneComponent* ImpulsePoint,
        ACMPlayerState* ContributingPlayerState,
        float MovementImpulseMultiplier
    );

    /** Applies one server-authoritative impulse toward a fixed hook anchor. */
    bool ApplyAnchorPull(
        ACMChimera& Chimera,
        int32 SegmentIndex,
        const FVector& AnchorLocation,
        float PullImpulse,
        float StopDistance
    );

    /** Applies the existing horizontal speed cap during the server physics Tick. */
    void UpdateServerMovement(ACMChimera& Chimera);

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    bool ApplyArmImpulse(
        ACMChimera& Chimera,
        UStaticMeshComponent* SegmentBody,
        USceneComponent* ImpulsePoint,
        ACMPlayerState* ContributingPlayerState,
        float MovementImpulseMultiplier
    );

    void RegisterCooperativeInput(
        ACMChimera& Chimera,
        const struct FCMPartSlotAddress& PartSlotAddress,
        ACMPlayerState* ContributingPlayerState,
        const FVector& PlanarImpulse
    );
    void MatchCooperativeInputs(ACMChimera& Chimera);
    void ApplyCooperativeForwardImpulse(
        ACMChimera& Chimera,
        float ForwardImpulseMagnitude
    ) const;
    void ApplyWholeBodyYawAssist(
        ACMChimera& Chimera,
        const struct FCMPartSlotAddress& PartSlotAddress,
        float MovementImpulseMultiplier
    ) const;
    void PurgeExpiredCooperativeInputs(double CurrentTime);
    void ScheduleNextCooperativeExpiry(ACMChimera& Chimera);
    void HandleCooperativeInputExpiry();

    bool TraceGroundAtPoint(
        const ACMChimera& Chimera,
        const FVector& DesiredFootPoint,
        const AActor* IgnoredPart,
        FHitResult& OutHit
    ) const;

    void ApplyActiveLegSteps(ACMChimera& Chimera);

    float GetPlayerCountSpeedMultiplier(
        const ACMChimera& Chimera
    ) const;
    float GetPerControlImpulseMultiplier(
        const ACMChimera& Chimera
    ) const;

    struct FPendingCooperativeImpulse
    {
        int32 FlatSlotIndex = INDEX_NONE;
        float RemainingImpulse = 0.0f;
        double ExpireTime = 0.0;
    };

    struct FActiveLegStep
    {
        TWeakObjectPtr<ACMLegPart> LegPart;
        TWeakObjectPtr<UStaticMeshComponent> SegmentBody;
        int32 SegmentIndex = INDEX_NONE;
        FVector VirtualFootPoint = FVector::ZeroVector;
        FVector GroundPoint = FVector::ZeroVector;
        FVector GroundNormal = FVector::UpVector;
        FVector PushForce = FVector::ZeroVector;
        double EndTime = 0.0;
    };

    // Each remaining input keeps its original expiry even after partial use.
    TArray<FPendingCooperativeImpulse> PendingLeftInputs;
    TArray<FPendingCooperativeImpulse> PendingRightInputs;
    TArray<FActiveLegStep> ActiveLegSteps;
    FTimerHandle CooperationExpiryTimerHandle;
};
