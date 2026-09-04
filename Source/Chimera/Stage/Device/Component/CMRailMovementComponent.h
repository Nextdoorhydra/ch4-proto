#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Parts/Arm/CMArmHoldTarget.h"
#include "CMRailMovementComponent.generated.h"

class USplineComponent;
class UBoxComponent;
class ACMArmPart;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCMRailProgressSignature, float, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCMRailEndpointSignature, bool, bAtEnd);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCMRailBlockedSignature);

// One authoritative distance on an open spline. The owner forwards its arm-hold interface here.
UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
class CHIMERA_API UCMRailMovementComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMRailMovementComponent();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category = "Chimera|Rail")
    void ConfigureRail(USplineComponent* InRail, UBoxComponent* InMovingBody, UPrimitiveComponent* InHandle);

    bool QueryArmHold(ACMArmPart* Arm, FCMArmHoldSpec& OutSpec) const;
    bool BeginArmHold(ACMArmPart* Arm);
    void EndArmHold(ACMArmPart* Arm);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Rail")
    void SetInteractionEnabled(bool bEnabled);

    // Optional mechanism-driven travel; physical arm holds use the same collision path.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Rail")
    void MoveToProgress(float TargetProgress);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Rail")
    void StopMovement();

    // Stage reset intentionally restores the authored pose without a collision sweep.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Rail")
    void ResetRail();

    UFUNCTION(BlueprintPure, Category = "Chimera|Rail")
    float GetProgress() const { return Progress; }

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Rail", meta = (ClampMin = "0", ClampMax = "1"))
    float InitialProgress = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Rail")
    bool bFollowRailRotation = false;

    // Relative to the spline component, or its tangent frame when following rotation.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Rail")
    FRotator RotationOffset = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Rail", meta = (ClampMin = "0.01"))
    float PullSensitivity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Rail", meta = (ClampMin = "1", Units = "cm/s"))
    float MaxMoveSpeed = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Rail", meta = (ClampMin = "0.1", ClampMax = "10", Units = "cm"))
    float CollisionStepDistance = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Rail", meta = (ClampMin = "1", Units = "cm"))
    float ReleaseDistance = 250.0f;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Rail")
    FCMRailProgressSignature OnProgressChanged;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Rail")
    FCMRailEndpointSignature OnEndpointReached;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Rail")
    FCMRailBlockedSignature OnMovementBlocked;

    // Editor preview only; never writes replicated state.
    static void PreviewPose(USplineComponent* Rail, UBoxComponent* Body, float AtProgress,
        bool bFollowRotation, const FRotator& Offset);

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FCMRailMovementTest;
#endif
    bool IsConfigured() const;
    bool AdvanceDistance(float DeltaDistance);
    bool IsSegmentBlocked(float FromDistance, float ToDistance) const;
    void ApplyPose();
    void PublishProgress(float PreviousProgress);
    void ReleaseArm();
    UFUNCTION()
    void OnRep_Progress();

    UPROPERTY(Transient)
    TObjectPtr<USplineComponent> Rail;
    UPROPERTY(Transient)
    TObjectPtr<UBoxComponent> MovingBody;
    UPROPERTY(Transient)
    TObjectPtr<UPrimitiveComponent> Handle;
    UPROPERTY(ReplicatedUsing = OnRep_Progress)
    float Progress = 0.0f;

    TWeakObjectPtr<ACMArmPart> HoldingArm;
    FVector PreviousArmLocation = FVector::ZeroVector;
    bool bTrackingHold = false;
    bool bInteractionEnabled = true;
    bool bAutomaticMove = false;
    float AutomaticTarget = 0.0f;
};
