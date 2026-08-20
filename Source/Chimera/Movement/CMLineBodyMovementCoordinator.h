#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CMLineBodyMovementCoordinator.generated.h"

class ACMChimera;
class ACMPlayerState;
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

    /** Returns true only when a valid grounded leg impulse was applied. */
    bool TryActivateLeg(
        ACMChimera& Chimera,
        int32 SegmentIndex,
        USceneComponent* FootPoint,
        ACMPlayerState* ContributingPlayerState,
        float MovementImpulseMultiplier
    );

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
    void UpdateServerMovement(ACMChimera& Chimera) const;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    bool ApplyLegImpulse(
        ACMChimera& Chimera,
        UStaticMeshComponent* SegmentBody,
        USceneComponent* FootPoint,
        ACMPlayerState* ContributingPlayerState,
        float MovementImpulseMultiplier
    );

    bool ApplyArmImpulse(
        ACMChimera& Chimera,
        UStaticMeshComponent* SegmentBody,
        USceneComponent* ImpulsePoint,
        ACMPlayerState* ContributingPlayerState,
        float MovementImpulseMultiplier
    );

    void RegisterCooperativeInput(
        ACMChimera& Chimera,
        ACMPlayerState* ContributingPlayerState,
        const FVector& PlanarImpulse,
        float YawAngularImpulse
    );
    void FlushCooperativeInput();

    bool TraceGround(
        const ACMChimera& Chimera,
        USceneComponent* FootPoint,
        FHitResult& OutHit
    ) const;

    float GetPlayerCountSpeedMultiplier(
        const ACMChimera& Chimera
    ) const;
    float GetPerControlImpulseMultiplier(
        const ACMChimera& Chimera
    ) const;

    TMap<int32, FVector> PendingCooperationContributions;
    TMap<int32, float> PendingCooperationYawImpulses;
    FTimerHandle CooperationFlushTimerHandle;
};
