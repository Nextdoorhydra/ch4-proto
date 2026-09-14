#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Parts/Arm/CMArmHoldTarget.h"
#include "CMRailMovementComponent.generated.h"

class USplineComponent;
class UBoxComponent;
class USceneComponent;
class ACMArmPart;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCMRailProgressSignature, float, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCMRailEndpointSignature, bool, bAtEnd);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCMRailBlockedSignature);

// Open spline segments form an endpoint-connected graph with one replicated active segment and progress.
UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
class CHIMERA_API UCMRailMovementComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMRailMovementComponent();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category = "Chimera|Rail")
    void ConfigureRail(USplineComponent* InRail, UBoxComponent* InMovingBody,
        UPrimitiveComponent* InHandle, USceneComponent* InVisual = nullptr);

    UFUNCTION(BlueprintCallable, Category = "Chimera|Rail")
    void ConfigureBranchedRail(
        USplineComponent* InEntryRail,
        const TArray<USplineComponent*>& InBranchRails,
        UBoxComponent* InMovingBody,
        UPrimitiveComponent* InHandle,
        USceneComponent* InVisual = nullptr);

    UFUNCTION(BlueprintCallable, Category = "Chimera|Rail")
    void ConfigureRailGraph(
        const TArray<USplineComponent*>& InRailSegments,
        UBoxComponent* InMovingBody,
        UPrimitiveComponent* InHandle,
        USceneComponent* InVisual = nullptr);

    bool QueryArmHold(ACMArmPart* Arm, FCMArmHoldSpec& OutSpec) const;
    bool CanArmHold(ACMArmPart* Arm) const;
    bool BeginArmHold(ACMArmPart* Arm);
    void EndArmHold(ACMArmPart* Arm);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Rail")
    void SetInteractionEnabled(bool bEnabled);

    // Optional mechanism-driven travel; physical arm holds use the same collision path.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Rail")
    void MoveToProgress(float TargetProgress);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Rail")
    void StopMovement();

    // 접촉한 액터가 미는 XY 방향으로 레일 바디를 고정 속도로 민다.
    bool TryCollisionPush(
        AActor* PushingActor,
        FVector WorldPushDirection,
        float DeltaTime);

    // Stage reset intentionally restores the authored pose without a collision sweep.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Rail")
    void ResetRail();

    UFUNCTION(BlueprintPure, Category = "Chimera|Rail")
    float GetProgress() const { return Progress; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Rail")
    int32 GetActiveSegmentIndex() const { return ActiveSegmentIndex; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Rail", meta = (DeprecatedFunction,
        DeprecationMessage = "Use GetActiveSegmentIndex."))
    int32 GetActiveBranchIndex() const { return ActiveSegmentIndex - 1; }

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Rail", meta = (ClampMin = "0"))
    int32 InitialSegmentIndex = 0;

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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Rail",
        meta = (ClampMin = "0.1"))
    float MovementSmoothingSpeed = 12.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Rail|Push")
    bool bAllowCollisionPush = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Rail|Push",
        meta = (ClampMin = "0.01", Units = "cm/s", EditCondition = "bAllowCollisionPush"))
    float CollisionPushSpeed = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Rail|Graph",
        meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float BranchSelectionDotThreshold = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Rail|Graph",
        meta = (ClampMin = "0.0", Units = "cm"))
    float RailConnectionTolerance = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Rail", meta = (ClampMin = "0.1", ClampMax = "10", Units = "cm"))
    float CollisionStepDistance = 2.0f;

    // Actor-specific reach override. The arm's normal reach still applies when it is larger.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Rail|Hold", meta = (ClampMin = "1", Units = "cm"))
    float MaximumGrabDistance = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Rail|Hold", meta = (ClampMin = "1", Units = "cm"))
    float ReleaseDistance = 400.0f;

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
    USplineComponent* GetActiveRail() const;
    bool IsAtConnectedJunction() const;
    bool IsEndpointConnected(int32 SegmentIndex, bool bAtEnd) const;
    bool AdvanceFromWorldDelta(FVector WorldDelta, const AActor* IgnoredActor = nullptr);
    bool SelectRailAtEndpoint(FVector WorldDirection);
    bool AdvanceDistance(float DeltaDistance, const AActor* IgnoredActor = nullptr);
    bool IsSegmentBlocked(float FromDistance, float ToDistance, const AActor* IgnoredActor) const;
    void ApplyPose(bool bInstant = false);
    void SmoothVisual(float DeltaTime);
    void PublishProgress(float PreviousProgress);
    void ReleaseArm();
    UFUNCTION()
    void OnRep_Progress();

    UPROPERTY(Transient)
    TArray<TObjectPtr<USplineComponent>> RailSegments;
    UPROPERTY(Transient)
    TObjectPtr<UBoxComponent> MovingBody;
    UPROPERTY(Transient)
    TObjectPtr<UPrimitiveComponent> Handle;
    UPROPERTY(Transient)
    TObjectPtr<USceneComponent> Visual;
    UPROPERTY(ReplicatedUsing = OnRep_Progress)
    float Progress = 0.0f;
    UPROPERTY(ReplicatedUsing = OnRep_Progress)
    int32 ActiveSegmentIndex = 0;

    TWeakObjectPtr<ACMArmPart> HoldingArm;
    FVector PreviousArmLocation = FVector::ZeroVector;
    bool bTrackingHold = false;
    bool bInteractionEnabled = true;
    bool bAutomaticMove = false;
    float AutomaticTarget = 0.0f;
    FTransform VisualTargetRelativeTransform = FTransform::Identity;
    bool bVisualSmoothing = false;
};
