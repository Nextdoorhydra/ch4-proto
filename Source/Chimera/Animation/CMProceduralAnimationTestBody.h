#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/CMPartSlotComponent.h"

#include "CMProceduralAnimationTestBody.generated.h"

class ACMPartActorBase;
class ACMArmPart;
class ACMLegPart;
class UBoxComponent;
class UCameraComponent;
class UChildActorComponent;
class UPhysicsConstraintComponent;
class USceneComponent;

/**
 * Isolated body used to tune procedural Part animation before ACMChimera.
 *
 * Eight independent box segments own physics and expose two Part slots each,
 * matching ACMChimera. Visual meshes are authored entirely in Blueprint so
 * designers can add any number of Skeletal Mesh components and edit their
 * transforms without native construction code overwriting them.
 */
UCLASS(Blueprintable)
class CHIMERA_API ACMProceduralAnimationTestBody : public AActor
{
    GENERATED_BODY()

public:
    ACMProceduralAnimationTestBody();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void PreInitializeComponents() override;
    virtual void BeginPlay() override;
    virtual void Destroyed() override;
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintPure, Category = "Chimera|Animation Test")
    UBoxComponent* GetPhysicsBody() const { return PhysicsBody; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Animation Test")
    USceneComponent* GetVisualRoot() const { return VisualRoot; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Animation Test|Camera")
    UCameraComponent* GetTopViewCamera() const { return TopViewCamera; }

    int32 GetBodySegmentCount() const { return BodySegments.Num(); }

    UBoxComponent* GetBodySegment(int32 Index) const
    {
        return BodySegments.IsValidIndex(Index)
            ? BodySegments[Index]
            : nullptr;
    }

    int32 GetPartSlotCount() const { return PartSlots.Num(); }

    UCMPartSlotComponent* GetPartSlot(int32 Index) const
    {
        return PartSlots.IsValidIndex(Index) ? PartSlots[Index] : nullptr;
    }

    UFUNCTION(BlueprintPure,
        Category = "Chimera|Animation Test|Control Rig")
    USceneComponent* GetLegRigControlAnchor(int32 Index) const
    {
        return LegRigControlAnchors.IsValidIndex(Index)
            ? LegRigControlAnchors[Index]
            : nullptr;
    }

    int32 GetSegmentConstraintCount() const
    {
        return SegmentConstraints.Num();
    }

    UPhysicsConstraintComponent* GetSegmentConstraint(int32 Index) const
    {
        return SegmentConstraints.IsValidIndex(Index)
            ? SegmentConstraints[Index]
            : nullptr;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Animation Test")
    USceneComponent* GetBodyControlTarget() const
    {
        return BodyControlTarget;
    }

    int32 GetSimulatedButtonPressCount() const
    {
        return SimulatedButtonPressCount;
    }

    int32 GetSimulatedAcceptedInputCount() const
    {
        return SimulatedAcceptedInputCount;
    }

    int32 GetSimulatedReverseButtonPressCount() const
    {
        return SimulatedReverseButtonPressCount;
    }

    int32 GetSimulatedPauseTurnButtonPressCount() const
    {
        return SimulatedPauseTurnButtonPressCount;
    }

    int32 GetSimulatedPauseTurnAcceptedInputCount() const
    {
        return SimulatedPauseTurnAcceptedInputCount;
    }

    bool IsSimulatedInputPaused() const
    {
        return bSimulatedInputPaused;
    }

    float GetSimulatedInputPauseDuration() const
    {
        return SimulatedInputPauseDuration;
    }

    int32 GetSimulatedVisualReplantCount() const
    {
        return SimulatedVisualReplantCount;
    }

    float GetSimulatedMaxReachError() const
    {
        return SimulatedMaxReachError;
    }

    /** Number of attached leg Parts currently holding a planted contact. */
    int32 GetSimulatedPlantedLegCount() const;

    /** Number of attached leg Parts ready to participate in the test. */
    int32 GetSimulatedOperationalLegCount() const;

    /** Number of attached Arm Parts participating in the input pattern. */
    int32 GetSimulatedOperationalArmCount() const;

    UFUNCTION(BlueprintCallable, Category = "Chimera|Animation Test|IK")
    void SetOneSidedLeanTestMode(bool bEnabled);

    UFUNCTION(BlueprintPure, Category = "Chimera|Animation Test|IK")
    bool IsOneSidedLeanTestModeEnabled() const
    {
        return bSimulateOneSidedBodyLean;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Animation Test")
    UCMPartSlotComponent* GetLeftPartSlot() const { return LeftPartSlot; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Animation Test")
    UCMPartSlotComponent* GetRightPartSlot() const { return RightPartSlot; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Animation Test")
    UChildActorComponent* GetLeftPartPreview() const
    {
        return LeftPartPreview;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Animation Test")
    UChildActorComponent* GetRightPartPreview() const
    {
        return RightPartPreview;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Animation Test")
    ACMPartActorBase* GetLeftPart() const { return LeftPart.Get(); }

    UFUNCTION(BlueprintPure, Category = "Chimera|Animation Test")
    ACMPartActorBase* GetRightPart() const { return RightPart.Get(); }

    /** Recreates configured Parts on all 8 x 2 body slots. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Animation Test")
    void RespawnConfiguredParts();

    /** Applies an impulse only to the physical proxy, never to visual meshes. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Animation Test")
    void AddBodyImpulse(FVector Impulse, bool bVelocityChange = false);

    static bool IsSupportedPartType(ECMPartSlotType PartType);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> PhysicsBody;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TArray<TObjectPtr<UBoxComponent>> BodySegments;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> VisualRoot;

    /** Runtime test camera that follows the centre of the articulated body. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UCameraComponent> TopViewCamera;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Camera")
    bool bAutoActivateTopViewCamera = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Camera",
        meta = (ClampMin = "100.0"))
    float TopViewCameraHeight = 1600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Camera",
        meta = (ClampMin = "5.0", ClampMax = "170.0"))
    float TopViewCameraFieldOfView = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Body",
        meta = (ClampMin = "1.0"))
    float BodySegmentSpacing = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Body",
        meta = (ClampMin = "1.0"))
    float BodyCollisionHalfLength = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Body",
        meta = (ClampMin = "1.0"))
    float BodyCollisionHalfWidth = 40.0f;

    /** Editable collision height used to align body and feet with the floor. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Body",
        meta = (ClampMin = "1.0"))
    float BodyCollisionHalfHeight = 35.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Body")
    float BodyCollisionVerticalOffset = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Body",
        meta = (ClampMin = "1.0"))
    float BodySlotLateralOffset = 60.0f;

    /** Target that future Part Control Rigs can use for body-height tuning. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> BodyControlTarget;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UCMPartSlotComponent> LeftPartSlot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UCMPartSlotComponent> RightPartSlot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TArray<TObjectPtr<UCMPartSlotComponent>> LeftPartSlots;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TArray<TObjectPtr<UCMPartSlotComponent>> RightPartSlots;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TArray<TObjectPtr<UCMPartSlotComponent>> PartSlots;

    /** Per-slot thigh attachment targets editable in the Blueprint viewport. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Components|Leg Control Rig")
    TArray<TObjectPtr<USceneComponent>> LegRigControlAnchors;

    /** Construction-time preview of the actual configured Part Blueprint. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UChildActorComponent> LeftPartPreview;

    /** Construction-time preview; the left-authored Part is mirrored here. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UChildActorComponent> RightPartPreview;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TArray<TObjectPtr<UChildActorComponent>> LeftPartPreviews;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TArray<TObjectPtr<UChildActorComponent>> RightPartPreviews;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TArray<TObjectPtr<UChildActorComponent>> PartPreviews;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TArray<TObjectPtr<UPhysicsConstraintComponent>> SegmentConstraints;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Parts")
    TSubclassOf<ACMPartActorBase> LeftPartClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Parts")
    TSubclassOf<ACMPartActorBase> RightPartClass;

    /** Arm Blueprint used instead of the default Part on selected segments. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Parts|Arm Test")
    TSubclassOf<ACMPartActorBase> ArmPartClass;

    /** Head Blueprint replacing one Arm slot for mouse-aim observation. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Parts|Head Test")
    TSubclassOf<ACMPartActorBase> HeadPartClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Parts|Head Test",
        meta = (ClampMin = "0", ClampMax = "7"))
    int32 SimulatedHeadSegmentIndex = 5;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Parts|Head Test")
    bool bSimulatedHeadOnRightSide = true;

    /** Both left/right slots on these body segments use ArmPartClass. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Parts|Arm Test")
    TArray<int32> SimulatedArmSegmentIndices = { 2, 5 };

    /** Mirrors the authored left Part when it is mounted in the right slot. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Parts")
    FVector RightPartRelativeScale = FVector(1.0, -1.0, 1.0);

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Parts")
    bool bSpawnConfiguredPartsOnBeginPlay = true;

    /** Lift an initially overlapping physics proxy above the first floor hit. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Physics")
    bool bResolveInitialGroundPenetration = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Physics",
        meta = (ClampMin = "0.0"))
    float InitialGroundClearance = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Physics",
        meta = (ClampMin = "1.0"))
    float InitialGroundProbeDistance = 500.0f;

    /** Eight real Chimera segments at the configured 35 kg per segment. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Physics",
        meta = (ClampMin = "1.0"))
    float SimulatedBodyMassKg = 280.0f;

    /** Matches ACMChimera: every simulated segment settles onto terrain. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Physics")
    bool bEnableBodyGravity = true;

    /** Matches ACMChimera's body damping rather than the old loose test proxy. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Physics",
        meta = (ClampMin = "0.0"))
    float BodyLinearDamping = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Physics",
        meta = (ClampMin = "0.0"))
    float BodyAngularDamping = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Physics")
    bool bLockBodyRoll = true;

    /** Optional diagnostic mode: lean one side to exercise planted-foot recovery. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|IK|One-Sided Lean")
    bool bSimulateOneSidedBodyLean = false;

    /** -1 leans toward the authored left side, +1 toward the right side. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|IK|One-Sided Lean",
        meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float SimulatedOneSidedLeanSide = -1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|IK|One-Sided Lean",
        meta = (ClampMin = "0.0", ClampMax = "80.0"))
    float SimulatedOneSidedLeanAngleDegrees = 35.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|IK|One-Sided Lean",
        meta = (ClampMin = "0.0"))
    float SimulatedOneSidedLeanResponse = 6.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|IK|One-Sided Lean",
        meta = (ClampMin = "0.0"))
    float SimulatedOneSidedLeanMaxAngularSpeedDegrees = 60.0f;

    /** Lower diagnostic threshold so a leaned, planted foot reaches recovery. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|IK|One-Sided Lean",
        meta = (ClampMin = "1.0"))
    float SimulatedOneSidedLeanReplantReleaseDistance = 15.0f;

    /** Server simulates four players, each attempting one assigned key per Tick. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Input")
    bool bSimulatePartInputEveryTick = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Input",
        meta = (ClampMin = "0.0"))
    float SimulatedLegStepDistance = 100.0f;

    /** Matches the sustained force produced by a real Leg key activation. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Input",
        meta = (ClampMin = "0.0"))
    float SimulatedLegForceScale = 3.0f;

    /** Production-equivalent per-segment limit for a matched left/right leg input. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Input",
        meta = (ClampMin = "0.0"))
    float SimulatedMaximumCooperativePlanarImpulse = 10000.0f;

    /** Matches ACMChimera's left/right cooperative input window. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Input",
        meta = (ClampMin = "0.01"))
    float SimulatedCooperationInputWindow = 0.20f;

    /** Time spent accepting simulated player input before an IK observation pause. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Input",
        meta = (ClampMin = "0.01"))
    float SimulatedInputMoveDuration = 3.0f;

    /** Observation/turn window after each simulated movement phase. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Input",
        meta = (ClampMin = "0.0"))
    float SimulatedInputPauseDuration = 5.0f;

    /** Three body segments whose paired legs turn the body clockwise while paused. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Input|Pause Turn")
    TArray<int32> SimulatedPauseTurnSegmentIndices = { 0, 3, 6 };

    /** Extra leg-force multiplier used only by the selected pause-turn legs. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Input|Pause Turn",
        meta = (ClampMin = "0.0"))
    float SimulatedPauseTurnForceScale = 6.0f;

    /** Reach error that releases a planted foot during the pause-turn test. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Input|Pause Turn",
        meta = (ClampMin = "1.0"))
    float SimulatedPauseTurnReplantReleaseDistance = 45.0f;

    /** Physics drag used to settle the body during the IK observation pause. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Input",
        meta = (ClampMin = "0.0"))
    float SimulatedPausePlanarBraking = 6.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Leg|Planted Foot",
        meta = (ClampMin = "0.0"))
    float SimulatedLegReplantReleaseDistance = 45.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Leg|Planted Foot",
        meta = (ClampMin = "0.01"))
    float SimulatedLegReplantDuration = 0.25f;

    /** Planar body speed at which reach recovery uses its fastest cadence. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Leg|Planted Foot|Fast Movement",
        meta = (ClampMin = "1.0"))
    float SimulatedFastLegSpeedThreshold = 300.0f;

    /** Shortest visible replant; five frames at 60 fps by default. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Leg|Planted Foot|Fast Movement",
        meta = (ClampMin = "0.05"))
    float SimulatedFastLegReplantDuration = 0.08f;

    /** Predicts where the body will be when a fast replant lands. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Leg|Planted Foot|Fast Movement",
        meta = (ClampMin = "0.0"))
    float SimulatedFastLegTargetLeadTime = 0.12f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Input",
        meta = (ClampMin = "1.0"))
    float SimulatedGroundTraceDistance = 500.0f;

    /** Matches ACMChimera's production sphere-sweep ground query. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Leg|Ground Trace",
        meta = (ClampMin = "0.0"))
    float GroundTraceHeight = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Leg|Ground Trace",
        meta = (ClampMin = "1.0"))
    float GroundTraceDepth = 140.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Leg|Ground Trace",
        meta = (ClampMin = "0.1"))
    float GroundCheckRadius = 12.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Leg|Ground Trace",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumGroundNormalZ = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Leg|Ground Trace")
    TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_Visibility;

    /** Delay between the alternating left/right turn input windows. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Input",
        meta = (ClampMin = "0.0"))
    float SimulatedTurnDelay = 0.75f;

    /** Duration for which a queued turn key is held. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Input",
        meta = (ClampMin = "0.0"))
    float SimulatedTurnDuration = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Input",
        meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float SimulatedHeadPitch = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Animation Test|Input",
        meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float SimulatedHeadYaw = 88.0f;

private:
    ACMPartActorBase* SpawnPart(
        TSubclassOf<ACMPartActorBase> PartClass,
        UCMPartSlotComponent* PartSlot,
        const FVector& RelativeScale
    );

    void ClearSpawnedParts();
    void RefreshBodyAssembly();
    void RefreshPartPreviews();
    void DestroyPartPreviews();
    void DestroySegmentConstraints();
    void ConfigureSegmentConstraints();
    void ConfigureBodyRotationLock(UBoxComponent* SegmentBody);
    void ResolveInitialGroundPenetration();
    void ApplySimulatedOneSidedLean(float DeltaSeconds);
    void AdvanceSimulatedMovementCycle(float DeltaSeconds);
    void ApplySimulatedPauseBraking(float DeltaSeconds);
    void SimulatePausedClockwiseRotation();
    void StopSimulatedNonTurnLegActions();
    void AdvanceSimulatedInputPhase(float DeltaSeconds);
    void UpdateTopViewCamera();
    void ActivateTopViewCamera();
    void UpdateMouseAimedHeads();
    void BindManualArmTestInput();
    void ManualArmControlKeyPressed(FKey Key);
    void ManualArmControlKeyReleased(FKey Key);
    UCMPartSlotComponent* GetManualArmPartSlot(int32 ArmIndex) const;
    bool TryBeginManualArmHold(
        ACMArmPart& ArmPart,
        const UCMPartSlotComponent& PartSlot
    );
    void EndManualArmHold(ACMArmPart& ArmPart);
    void DestroyManualArmGroundConstraints();
    TSubclassOf<ACMPartActorBase> ResolvePartClassForSegment(
        int32 SegmentIndex,
        bool bRightSide
    ) const;
    UCMPartSlotComponent* GetSimulatedPlayerPartSlot(
        int32 PlayerIndex,
        int32 KeyIndex
    ) const;
    void UpdateSimulatedPartAction(
        UCMPartSlotComponent* PartSlot,
        float DeltaSeconds
    );
    void UpdateSimulatedLegReachRecovery();
    bool SimulatePartButtonPress(
        UCMPartSlotComponent* PartSlot,
        bool bReverseMovement
    );
    bool TraceGroundAtPoint(
        const FVector& DesiredFootPoint,
        const AActor* IgnoredPart,
        FHitResult& OutHit
    ) const;
    bool TraceReachableLegGround(
        const UCMPartSlotComponent& PartSlot,
        const ACMLegPart& LegPart,
        float ForwardOffset,
        FHitResult& OutHit
    ) const;
    void ApplySimulatedLegForce(
        const ACMLegPart& LegPart,
        const UCMPartSlotComponent& PartSlot,
        bool bReverseMovement
    );
    void RegisterSimulatedCooperativeInput(
        bool bIsLeft,
        float MovementImpulse,
        bool bReverseMovement
    );
    void ApplySimulatedCooperativeImpulse(float SignedForwardImpulse);

    struct FPendingSimulatedCooperativeInput
    {
        float RemainingImpulse = 0.0f;
        bool bReverseMovement = false;
        float ExpireTime = 0.0f;
    };

    struct FSimulatedLegReplantPath
    {
        bool bInitialized = false;
        FVector LastReferenceLocation = FVector::ZeroVector;
        float DistanceTowardNextStep = 0.0f;
        int32 PendingStepCount = 0;
    };

    struct FSimulatedLegForceAction
    {
        float RemainingTime = 0.0f;
        bool bReverseMovement = false;
    };

    UPROPERTY(Transient)
    TObjectPtr<ACMPartActorBase> LeftPart;

    UPROPERTY(Transient)
    TObjectPtr<ACMPartActorBase> RightPart;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ACMPartActorBase>> SpawnedParts;

    float SimulatedTurnClock = 0.0f;
    bool bSimulatedTurnActive = false;
    int32 SimulatedTurnSign = -1;
    int32 SimulatedInputPlayerCursor = 0;
    int32 SimulatedPlayerKeyIndices[4] = { 0, 0, 0, 0 };
    int32 SimulatedButtonPressCount = 0;
    int32 SimulatedAcceptedInputCount = 0;
    int32 SimulatedReverseButtonPressCount = 0;
    int32 SimulatedPauseTurnButtonPressCount = 0;
    int32 SimulatedPauseTurnAcceptedInputCount = 0;
    int32 SimulatedPauseTurnStepCursor = 0;
    int32 SimulatedVisualReplantCount = 0;
    float SimulatedMaxReachError = 0.0f;
    float SimulatedCooperationClock = 0.0f;
    float SimulatedMovementCycleClock = 0.0f;
    bool bSimulatedInputPaused = false;
    TArray<FPendingSimulatedCooperativeInput> PendingLeftCooperativeInputs;
    TArray<FPendingSimulatedCooperativeInput> PendingRightCooperativeInputs;
    TMap<TWeakObjectPtr<ACMLegPart>, FSimulatedLegReplantPath>
        SimulatedLegReplantPaths;
    TMap<TWeakObjectPtr<ACMLegPart>, FSimulatedLegForceAction>
        SimulatedLegForceActions;
    TMap<TWeakObjectPtr<ACMArmPart>,
        TWeakObjectPtr<UPhysicsConstraintComponent>>
        ManualArmGroundConstraints;
    bool bTopViewCameraActivated = false;
};
