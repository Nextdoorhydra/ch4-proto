#pragma once

#include "CoreMinimal.h"
#include "Parts/Arm/CMArmHoldTarget.h"
#include "Stage/Trigger/CMGrabPullTarget.h"
#include "Stage/Trigger/CMStageButtonBase.h"

#include "CMLeverBase.generated.h"

class UAudioComponent;

UCLASS(Blueprintable)
// 일반 팔의 홀드 이동으로 회전하며 양끝 임계점에서 상태를 전환하는 레버
class CHIMERA_API ACMLeverBase
    : public ACMStageButtonBase
    , public ICMGrabPullTarget
    , public ICMArmHoldTarget
{
    GENERATED_BODY()

public:
    ACMLeverBase();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual bool QueryArmHold_Implementation(ACMArmPart* ArmPart, FCMArmHoldSpec& OutSpec) const override;
    virtual bool BeginArmHold_Implementation(ACMArmPart* ArmPart) override;
    virtual void EndArmHold_Implementation(ACMArmPart* ArmPart) override;

    virtual bool TryHandlePull_Implementation(
        AActor* PullingActor,
        FVector PullOrigin,
        float PullStrength) override;

    virtual ECMGrabPullResult HandlePullWithResult_Implementation(
        AActor* PullingActor, FVector PullOrigin, float PullStrength) override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void HandleElementActiveChanged_Implementation(bool bIsActive) override;
    virtual void HandleElementReset_Implementation() override;

    // Attach the moving handle mesh beneath this pivot in the Blueprint.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> LeverPivot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<class USphereComponent> ArmHoldVolume;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera Lever", meta = (ClampMin = "1.0"))
    float FullTravelDistance = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera Lever", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float SwitchThreshold = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera Lever", meta = (ClampMin = "1.0", ClampMax = "89.0"))
    float RotationHalfAngle = 45.0f;

    // Time to visually rotate from one end to the other. Zero snaps immediately.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera Lever", meta = (ClampMin = "0.0", Units = "s"))
    float RotationTransitionDuration = 0.3f;

    // When enabled, releasing the arm deactivates the lever and returns it to its default pose.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera Lever")
    bool bRequiresHoldToStayActivated = false;

    // Releases the arm when it moves this far from the handle. Zero disables the limit.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera Lever",
        meta = (ClampMin = "0.0", Units = "cm"))
    float MaximumHoldDistance = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera Lever")
    FVector LocalRotationAxis = FVector::RightVector;

    UPROPERTY(ReplicatedUsing = OnRep_LeverAlpha, VisibleInstanceOnly, BlueprintReadOnly, Category = "Chimera Lever")
    float LeverAlpha = -1.0f;

    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera Lever")
    void OnLeverAlphaChanged(float NewAlpha);

    // Actual interpolated pose. Use for visual effects, not puzzle state.
    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera Lever")
    void OnLeverVisualAlphaChanged(float NewVisualAlpha);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Lever",
        meta = (ClampMin = "0.0"))
    float RequiredPullStrength = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Lever")
    FVector LocalPullAxis = FVector::ForwardVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Mechanism|Lever",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumPullAlignment = 0.5f;

    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Mechanism|Lever")
    void OnLeverPulled(AActor* PullingActor);

private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FCMLeverInteractionRegressionTest;
#endif
    UFUNCTION()
    void OnRep_LeverAlpha();
    void NotifyLeverTargetChanged();
    UFUNCTION()
    void HandleLeverTriggerChanged(AActor* TriggeringActor);
    void UpdateArmHold();
    void UpdateVisualRotation(float DeltaSeconds);
    void StartLeverMoveSound();
    void HandleLeverMoveSoundIdle();
    void StopLeverMoveSound();
    void PlayLeverSettleSound();
    void StopArmHold();
    bool IsHoldDistanceExceeded(const ACMArmPart& ArmPart) const;
    bool SetLeverPressed(bool bPressed, AActor* InstigatorActor);

    TWeakObjectPtr<ACMArmPart> HoldingArm;
    FVector GrabStartArmLocation = FVector::ZeroVector;
    float GrabStartAlpha = -1.0f;
    FQuat InitialPivotRotation = FQuat::Identity;
    bool bLeverPoseInitialized = false;
    bool bTrackingArmHold = false;
    float VisualLeverAlpha = -1.0f;
    bool bUpdatingFromHold = false;
    bool bLeverMovementSoundActive = false;
    ECMGrabPullResult LastPullResult = ECMGrabPullResult::Unhandled;

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> LeverMoveLoopComponent;

    FTimerHandle LeverMoveSoundStopTimerHandle;
};
