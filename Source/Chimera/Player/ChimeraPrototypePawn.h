#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "ChimeraControlTypes.h"
#include "ChimeraPrototypePawn.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UPhysicsConstraintComponent;
class USpringArmComponent;
class UCameraComponent;
class UMaterialInstanceDynamic;
class UTextRenderComponent;
class AChimeraPlayerState;

USTRUCT()
struct FChimeraReplicatedSegmentState
{
    GENERATED_BODY()

    UPROPERTY()
    FVector_NetQuantize100 Location = FVector::ZeroVector;

    UPROPERTY()
    FRotator Rotation = FRotator::ZeroRotator;
};

UCLASS()
class CHIMERA_API AChimeraPrototypePawn : public APawn
{
    GENERATED_BODY()

public:
    AChimeraPrototypePawn();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Controls")
    void ActivateControlPart(
        EChimeraControlPart ControlPart,
        AChimeraPlayerState* ContributingPlayerState
    );

    void SetControlPartPressed(
        EChimeraControlPart ControlPart,
        bool bPressed
    );

    void ClearPressedControlParts();

    FRotator GetInitialCameraRotation() const;
    float GetMouseLookSensitivity() const;
    bool IsMousePitchInverted() const;
    void SetLocalCameraRotation(const FRotator& NewCameraRotation);
    void AdjustLocalCameraZoom(float AxisValue);

protected:
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

public:
    virtual void Tick(float DeltaTime) override;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> BodyMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> LeftFootPoint;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> RightFootPoint;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera")
    TArray<TObjectPtr<UStaticMeshComponent>> BodySegments;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera")
    TArray<TObjectPtr<USceneComponent>> LeftFootPoints;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera")
    TArray<TObjectPtr<USceneComponent>> RightFootPoints;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera")
    TArray<TObjectPtr<UPhysicsConstraintComponent>> SegmentConstraints;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Chimera|Control Markers")
    TArray<TObjectPtr<UStaticMeshComponent>> ControlAssignmentMarkers;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Chimera|Control Markers")
    TArray<TObjectPtr<UTextRenderComponent>> ControlAssignmentMarkerTexts;

    UPROPERTY(EditAnywhere, Category = "Leg")
    float LegImpulse = 5000.0f;

    UPROPERTY(EditAnywhere, Category = "Leg|Ground Check",
        meta = (ClampMin = "1.0"))
    float GroundCheckRadius = 12.0f;

    UPROPERTY(EditAnywhere, Category = "Leg|Ground Check",
        meta = (ClampMin = "0.0"))
    float GroundContactDistance = 8.0f;

    UPROPERTY(EditAnywhere, Category = "Leg|Ground Check",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumGroundNormalZ = 0.5f;

    UPROPERTY(EditAnywhere, Category = "Leg")
    float MaxSpeed = 600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Leg|Cooperation", meta = (ClampMin = "0.01"))
    float CooperationInputWindow = 0.20f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Leg|Cooperation",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CooperationDirectionThreshold = 0.85f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Leg|Cooperation",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float IndividualPlanarTranslationFraction = 0.005f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Leg|Cooperation",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float IndividualYawRotationFraction = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Leg|Cooperation", meta = (ClampMin = "0.0"))
    float MaximumCooperativePlanarImpulse = 10000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Leg|Cooperation", meta = (ClampMin = "0.0"))
    float MaximumCooperativeYawAngularImpulse = 1000000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Leg|Player Scaling", meta = (ClampMin = "0.1"))
    float OnePlayerTurnTargetSeconds = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Leg|Player Scaling", meta = (ClampMin = "0.1"))
    float TwoPlayerTurnTargetSeconds = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Leg|Player Scaling", meta = (ClampMin = "0.1"))
    float ThreePlayerTurnTargetSeconds = 7.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Leg|Player Scaling", meta = (ClampMin = "0.1"))
    float FourPlayerTurnTargetSeconds = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera",
        meta = (ClampMin = "1", ClampMax = "4", UIMin = "1", UIMax = "4"))
    int32 ActiveSegmentCount = 4;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera",
        meta = (ClampMin = "0.1"))
    float SegmentScale = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera",
        meta = (ClampMin = "10.0"))
    float SegmentSpacing = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Feet")
    FVector LeftFootOffset = FVector(0.0f, -60.0f, -30.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Feet")
    FVector RightFootOffset = FVector(0.0f, 60.0f, -30.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Physics",
        meta = (ClampMin = "0.0"))
    float BodyLinearDamping = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Physics",
        meta = (ClampMin = "0.0"))
    float BodyAngularDamping = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Physics")
    bool bEnableBodyGravity = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Physics")
    bool bLockBodyUpright = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Physics")
    FName BodyCollisionProfile = TEXT("PhysicsActor");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Constraint",
        meta = (ClampMin = "0.0", ClampMax = "90.0"))
    float SwingLimitDegrees = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Constraint",
        meta = (ClampMin = "0.0", ClampMax = "90.0"))
    float TwistLimitDegrees = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Constraint")
    bool bDisableCollisionBetweenSegments = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera",
        meta = (ClampMin = "0.0"))
    float DefaultCameraDistance = 900.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera",
        meta = (ClampMin = "-89.0", ClampMax = "89.0"))
    float InitialCameraPitch = -25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
    FVector CameraSocketOffset = FVector(0.0f, 0.0f, 120.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera",
        meta = (ClampMin = "0.0"))
    float CameraFollowBackOffset = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Lag")
    bool bEnableCameraLag = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Lag",
        meta = (ClampMin = "0.0"))
    float CameraLagSpeed = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Lag",
        meta = (ClampMin = "0.0"))
    float CameraLagMaxDistance = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Lag")
    bool bEnableCameraRotationLag = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Lag",
        meta = (ClampMin = "0.0"))
    float CameraRotationLagSpeed = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
    bool bEnableCameraCollision = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera",
        meta = (ClampMin = "0.01"))
    float MouseLookSensitivity = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
    bool bInvertMousePitch = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera",
        meta = (ClampMin = "1.0"))
    float ZoomStep = 75.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera",
        meta = (ClampMin = "0.0"))
    float MinimumCameraDistance = 600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera",
        meta = (ClampMin = "0.0"))
    float MaximumCameraDistance = 1600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Control Markers", meta = (ClampMin = "1.0"))
    float ControlMarkerRadius = 14.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Control Markers", meta = (ClampMin = "0.1"))
    float ControlMarkerThickness = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Control Markers")
    float ControlMarkerHeightOffset = 85.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Control Markers")
    FName ControlMarkerColorParameter = TEXT("Color");

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Control Markers",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PressedMarkerBrightness = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Control Markers",
        meta = (ClampMin = "1.0"))
    float PressedMarkerScale = 1.3f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Control Markers",
        meta = (ClampMin = "0.0"))
    float MarkerScaleAnimationSpeed = 12.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Control Markers", meta = (ClampMin = "1.0"))
    float ControlMarkerTextWorldSize = 24.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Control Markers")
    float ControlMarkerTextHeightOffset = 12.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leg|Ground Check")
    TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_Visibility;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leg|Ground Check")
    bool bDrawGroundContactDebug = false;

private:
    void TryApplyLegImpulse(
        int32 SegmentIndex,
        bool bRightLeg,
        AChimeraPlayerState* ContributingPlayerState
    );

    void ApplyLegImpulse(
        UStaticMeshComponent* SegmentBody,
        USceneComponent* FootPoint,
        AChimeraPlayerState* ContributingPlayerState
    );

    void RegisterCooperativeInput(
        AChimeraPlayerState* ContributingPlayerState,
        const FVector& PlanarImpulse,
        float YawAngularImpulse
    );
    void FlushCooperativeInput();

    void ConfigureSegments();
    void ConfigureNetworkPhysics();
    void ConfigureBodyRotationLock(UStaticMeshComponent* SegmentBody);
    void ApplyBlueprintSettings();
    void UpdateCameraFollowOffset();
    void UpdateControlAssignmentMarkers(float DeltaTime);
    void UpdateReplicatedSegmentStates();
    void ApplyReplicatedSegmentStates(float DeltaTime);
    float GetPlayerCountSpeedMultiplier() const;
    float GetPerControlImpulseMultiplier() const;

    UFUNCTION()
    void OnRep_SegmentStates();

    bool TraceGround(
        USceneComponent* FootPoint,
        FHitResult& OutHit
    ) const;

    UPROPERTY(ReplicatedUsing = OnRep_SegmentStates)
    TArray<FChimeraReplicatedSegmentState> ReplicatedSegmentStates;

    UPROPERTY(Replicated)
    uint8 PressedControlPartMask = 0;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> ControlMarkerMaterials;

    TArray<int32> AppliedMarkerColorIndices;
    TArray<int32> AppliedMarkerSlotIndices;
    TArray<bool> AppliedMarkerPressedStates;
    TMap<int32, FVector> PendingCooperationContributions;
    TMap<int32, float> PendingCooperationYawImpulses;
    float CooperationWindowRemaining = 0.0f;
    bool bHasReceivedSegmentStates = false;
};
