#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Pawn.h"

#include "CMControlTypes.h"
#include "CMChimera.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class USceneComponent;
class UPhysicsConstraintComponent;
class USpringArmComponent;
class UCameraComponent;
class UCMCameraOcclusionComponent;
class UMaterialInstanceDynamic;
class UTextRenderComponent;
class UAbilitySystemComponent;
class UCMChimeraAttributeSet;
class UCMLineBodyMovementCoordinator;
class UCMPartSlotComponent;
class UPrimitiveComponent;
class UDataTable;
class UPhysicalMaterial;
class ACMPlayerState;
class ACMArmPart;
class ACMLegPart;
class ACMSpringArmPart;
class AActor;

DECLARE_LOG_CATEGORY_EXTERN(LogChimeraLineBody, Log, All);

/** Fired once on the authoritative Chimera when every active body segment is dead. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCMAllSegmentsDeadSignature);

/** Fired on the server whenever one body segment is destroyed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FCMSegmentDestroyedSignature,
    int32,
    SegmentIndex
);

USTRUCT()
struct FCMReplicatedSegmentState
{
    GENERATED_BODY()

    UPROPERTY()
    FVector_NetQuantize100 Location = FVector::ZeroVector;

    UPROPERTY()
    FRotator Rotation = FRotator::ZeroRotator;
};

USTRUCT(BlueprintType)
struct FCMBodySegmentHealthState
{
    GENERATED_BODY()

    // BodySegments 배열 및 Q/W/E/R 슬롯 매핑에 사용하는 0 기반 마디 번호다.
    UPROPERTY(BlueprintReadOnly)
    int32 SegmentIndex = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly)
    float Health = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float MaxHealth = 0.0f;

    // 체력이 0이 된 뒤 해당 마디에 연결된 두 조작 슬롯을 막는 명시적 상태다.
    UPROPERTY(BlueprintReadOnly)
    bool bDead = false;
};

UCLASS()
class CHIMERA_API ACMChimera
    : public APawn
    , public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ACMChimera();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent()
        const override;

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Controls")
    void ActivatePartSlot(
        const FCMPartSlotAddress& PartSlotAddress,
        ACMPlayerState* ContributingPlayerState,
        bool bReverseMovement
    );

    void SetPartSlotPressed(
        const FCMPartSlotAddress& PartSlotAddress,
        bool bPressed
    );

    // SpringArm을 제외한 디폴트 암 슬롯인지 확인한다.
    bool IsBasicArmPartSlot(
        const FCMPartSlotAddress& PartSlotAddress
    ) const;

    // 디폴트 암이 상호작 대상을 잡았다면 해제 공격을 억제한다.
    bool ShouldActivateBasicArmOnRelease(
        const FCMPartSlotAddress& PartSlotAddress
    ) const;

    void ClearPressedControlParts();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Health")
    void ApplyDamageToSegment(int32 SegmentIndex, float Damage);

    UFUNCTION(BlueprintPure, Category = "Chimera|Health")
    bool IsSegmentAlive(int32 SegmentIndex) const;

    /** Returns the stable body Segment index represented by a hurtbox hit. */
    UFUNCTION(BlueprintPure, Category = "Chimera|Health")
    int32 GetSegmentIndexFromHurtbox(
        const UPrimitiveComponent* HitComponent
    ) const;

    /** Returns the stable Segment index for either its body mesh or hurtbox. */
    UFUNCTION(BlueprintPure, Category = "Chimera|Health")
    int32 GetSegmentIndexFromDamageComponent(
        const UPrimitiveComponent* HitComponent
    ) const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Health")
    UBoxComponent* GetSegmentHurtbox(int32 SegmentIndex) const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Health")
    TArray<FCMBodySegmentHealthState> GetSegmentHealthStates() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Stamina")
    float GetStamina() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Stamina")
    float GetMaxStamina() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Health")
    bool AreAllSegmentsDead() const;

    // 체크포인트 리스폰을 위해 몸통 체력과 플레이어 입력 사망 상태 복구
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Health")
    void RestoreForCheckpointRespawn();

    /** Keeps the active body count equal to the 4~8 participating players. */
    void SetActiveSegmentCountForPlayers(int32 PlayerCount);

    UFUNCTION(BlueprintPure, Category = "Chimera")
    int32 GetActiveSegmentCount() const;

    /** Changes only this process's camera component; the value is not replicated. */
    float AdjustLocalCameraDistance(float WheelInput);

    // 지정 Volume과 모든 활성 몸통 물리 컴포넌트가 겹치는지 확인
    bool AreAllActiveBodySegmentsOverlapping(
        const UPrimitiveComponent* Volume) const;

    // 서버에서 받은 하나의 환경 Force를 활성 몸통 마디의 질량 비율로 분배
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Physics")
    void ApplyEnvironmentalForce(const FVector& TotalForce);

    // Test Area 이동을 위해 활성 몸통 마디의 상대 배치를 유지하며 전체 물리 조립체 이동
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Testing")
    bool TeleportAssembly(const FTransform& DestinationTransform);

    UFUNCTION(BlueprintPure, Category = "Chimera|Part Slots")
    UCMPartSlotComponent* GetPartSlotComponent(
        const FCMPartSlotAddress& PartSlotAddress
    ) const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Part Slots")
    bool IsPartSlotPressed(
        const FCMPartSlotAddress& PartSlotAddress
    ) const;

    /** Server-authoritative attachment entry point for pickup/UI systems. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Part Slots")
    bool AttachPartToSlot(
        const FCMPartSlotAddress& PartSlotAddress,
        AActor* PartActor
    );

    /** Server-authoritative detachment entry point. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Part Slots")
    AActor* DetachPartFromSlot(
        const FCMPartSlotAddress& PartSlotAddress
    );

    /** Server-side production entry point shared by concrete Leg abilities. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Movement")
    bool TryActivateLegPart(
        const FCMPartSlotAddress& PartSlotAddress,
        ACMPlayerState* ContributingPlayerState,
        bool bReverseMovement
    );

    /** Cancels the sustained push owned by one attached Leg. */
    void CancelLegStep(ACMLegPart* LegPart);

    /** Server-side production entry point shared by concrete Arm abilities. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Combat")
    bool TryActivateArmPart(
        ACMArmPart* ArmPart,
        ACMPlayerState* ContributingPlayerState
    );

    /** Pulls one simulated body segment toward a SpringArm hook anchor. */
    bool ApplySpringArmPull(
        const FCMPartSlotAddress& PartSlotAddress,
        const FVector& AnchorLocation,
        float PullImpulse,
        float StopDistance
    );

    /** Gives this SpringArm exclusive ownership of the Chimera pull. */
    bool RequestSpringArmPull(ACMSpringArmPart* SpringArm);

    /** Releases pull ownership only when this SpringArm currently owns it. */
    void ReleaseSpringArmPull(ACMSpringArmPart* SpringArm);

    /** True while one SpringArm owns the Chimera pull. */
    bool IsSpringArmPulling() const;
    
    /** Attaches registered production Part Blueprints to empty active slots. */
    void SpawnRandomDebugParts();

    /** Replaces one one-based debug slot with the requested production Part. */
    bool SpawnDebugPartAtSlot(int32 FlatSlotIndex, FName PartName);

    /** Replaces every active slot with the requested production Part. */
    void FillAllDebugSlotsWithPart(FName PartName);

    /** Removes only production Parts created by SpawnRandomDebugParts. */
    void ClearRandomDebugParts();

    /** Attaches production Leg Parts to every empty active slot for testing. */
    void SpawnTestLegParts();

    /** Removes only Leg Parts created by SpawnTestLegParts. */
    void ClearTestLegParts();

    /** Keeps the diagnostic Leg GA routed through the production movement API. */
    void ActivateDebugLegPart(
        const FCMPartSlotAddress& PartSlotAddress,
        ACMPlayerState* ContributingPlayerState
    );

    // GameMode/GameState may bind to this without CMChimera deciding the
    // project-wide defeat flow itself.
    UPROPERTY(BlueprintAssignable, Category = "Chimera|Health")
    FCMAllSegmentsDeadSignature OnAllSegmentsDead;

    // PlayerState/GameMode can bind this to mark only the player assigned to
    // the destroyed segment as defeated.
    UPROPERTY(BlueprintAssignable, Category = "Chimera|Health")
    FCMSegmentDestroyedSignature OnSegmentDestroyed;

protected:
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

public:
    virtual void Tick(float DeltaTime) override;

    // Non-Shipping 화살표 치트 입력을 마디 수와 질량에 무관한 가속도로 적용
    void ApplyDebugMovementInput(float ForwardInput, float TurnInput);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> BodyMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UCMPartSlotComponent> LeftFootPoint;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UCMPartSlotComponent> RightFootPoint;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    TObjectPtr<UCMCameraOcclusionComponent> CameraOcclusionComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Chimera|Abilities")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Chimera|Movement")
    TObjectPtr<UCMLineBodyMovementCoordinator> MovementCoordinator;

    // ASC가 소유하는 키메라 전체 공용 체력/스태미나 데이터다.
    UPROPERTY()
    TObjectPtr<UCMChimeraAttributeSet> AttributeSet;

    // Google Sheet Loader가 갱신하는 DT를 런타임에 느슨하게 참조한다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Data")
    TSoftObjectPtr<UDataTable> BodyDataTable;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Data")
    FName BodyRowName = TEXT("LineBody");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient,
        Category = "Chimera")
    TArray<TObjectPtr<UStaticMeshComponent>> BodySegments;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient,
        Category = "Chimera|Health")
    TArray<TObjectPtr<UBoxComponent>> SegmentHurtboxes;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient,
        Category = "Chimera")
    TArray<TObjectPtr<USceneComponent>> LeftFootPoints;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient,
        Category = "Chimera")
    TArray<TObjectPtr<USceneComponent>> RightFootPoints;

    /** Flattened as SegmentIndex * PartSlotsPerSegment + PartSlotIndex. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient,
        Category = "Chimera|Part Slots")
    TArray<TObjectPtr<UCMPartSlotComponent>> PartSlotPoints;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera")
    TArray<TObjectPtr<UPhysicsConstraintComponent>> SegmentConstraints;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient,
        Category = "Chimera|Control Markers")
    TArray<TObjectPtr<UStaticMeshComponent>> ControlAssignmentMarkers;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient,
        Category = "Chimera|Control Markers")
    TArray<TObjectPtr<UTextRenderComponent>> ControlAssignmentMarkerTexts;

    /** Forward reach of the animation-free Virtual Foot prototype. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leg|Step",
        meta = (ClampMin = "0.0"))
    float LegStepLength = 100.0f;

    /** Height above the desired foot point where the ground sweep starts. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leg|Step",
        meta = (ClampMin = "0.0"))
    float LegStepTraceHeight = 60.0f;

    /** Distance below the desired foot point covered by the ground sweep. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leg|Step",
        meta = (ClampMin = "0.0"))
    float LegStepTraceDepth = 140.0f;

    /** Scales the force converted from the existing impulse balance values. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leg|Step",
        meta = (ClampMin = "0.0"))
    float LegStepForceScale = 1.0f;

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
    Category = "Chimera|Movement|SpringArm",
    meta = (ClampMin = "0.0"))
    float SpringArmMaxSpeed = 4000.0f;

    UPROPERTY(EditAnywhere, Category = "Chimera|Debug Movement",
        meta = (ClampMin = "0.0"))
    // 모든 활성 마디에 적용하는 질량 독립적인 디버그 가속도
    float DebugMovementForce = 35000.0f;

    UPROPERTY(EditAnywhere, Category = "Chimera|Debug Movement",
        meta = (ClampMin = "0.0"))
    float DebugTurnTorque = 250000.0f;

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

    /** Whole-body yaw velocity change contributed by a Tier-1 input. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Movement|Turning",
        meta = (ClampMin = "0.0"))
    float YawAssistDegreesPerInput = 40.0f;

    /** Caps server yaw velocity so simultaneous inputs cannot spin the body. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Movement|Turning",
        meta = (ClampMin = "0.0"))
    float MaximumYawAngularSpeedDegrees = 90.0f;

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

    UPROPERTY(ReplicatedUsing = OnRep_ActiveSegmentCount,
        EditAnywhere, BlueprintReadOnly, Category = "Chimera",
        meta = (ClampMin = "1", ClampMax = "8", UIMin = "1", UIMax = "8"))
    int32 ActiveSegmentCount = 4;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera",
        meta = (ClampMin = "0.1"))
    float SegmentScale = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera",
        meta = (ClampMin = "10.0"))
    float SegmentSpacing = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Physics",
        meta = (ClampMin = "0.0"))
    float BodyLinearDamping = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Physics",
        meta = (ClampMin = "0.0"))
    float BodyAngularDamping = 5.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Physics")
    float BodySegmentMass = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Physics")
    float BodyGroundFriction = 0.0f;

    UPROPERTY(Transient)
    TObjectPtr<UPhysicalMaterial> RuntimeBodyPhysicalMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Physics")
    bool bEnableBodyGravity = true;

    /** Prevents each Segment from rolling sideways while allowing hills and turns. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Physics")
    bool bLockBodyRoll = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Physics")
    FName BodyCollisionProfile = TEXT("PhysicsActor");

    // 위아래 꺾임 정도
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Constraint",
        meta = (ClampMin = "0.0", ClampMax = "90.0"))
    float TerrainPitchLimitDegrees = 22.0f;

    // 좌우 꺾임 정도
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Constraint",
        meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float HorizontalBendLimitDegrees = 50.0f;

    // 비틀림 정도, 얘는 잠겨있음
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Constraint",
        meta = (ClampMin = "0.0", ClampMax = "90.0"))
    float TwistLimitDegrees = 8.0f;

    /** Damps relative Pitch/Yaw speed without pulling Segments straight. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Constraint",
        meta = (ClampMin = "0.0"))
    float SwingVelocityDamping = 5.0f;

    /** Zero means that Chaos does not cap the damping torque. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Constraint",
        meta = (ClampMin = "0.0"))
    float SwingDampingForceLimit = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Constraint")
    bool bDisableCollisionBetweenSegments = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera",
        meta = (ClampMin = "0.0"))
    float DefaultCameraDistance = 900.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Distance",
        meta = (ClampMin = "0.0"))
    float MinimumCameraDistance = 650.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Distance",
        meta = (ClampMin = "0.0"))
    float MaximumCameraDistance = 2200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Distance",
        meta = (ClampMin = "0.0"))
    float CameraDistanceStep = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera",
        meta = (ClampMin = "-89.0", ClampMax = "89.0"))
    float InitialCameraPitch = -65.0f;

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
    void ConfigureSegments();
    void ConfigureNetworkPhysics();
    void ConfigureBodyRotationLock(UStaticMeshComponent* SegmentBody);
    bool InitializeFromBodyData();
    void InitializeSharedAttributes(
        float MaxStamina,
        float StaminaRegen
    );
    void InitializeSegmentHealth(float SegmentMaxHealth);
    void StartStaminaRegeneration();
    void PauseStaminaRegeneration();
    void ApplyBlueprintSettings();
    void UpdateCameraFollowOffset();
    void UpdateControlAssignmentMarkers(float DeltaTime);
    void UpdateReplicatedSegmentStates();
    void ApplyReplicatedSegmentStates(float DeltaTime);

    UFUNCTION()
    void OnRep_SegmentStates();

    UFUNCTION()
    void OnRep_SegmentHealthStates();

    UFUNCTION()
    void OnRep_ActiveSegmentCount();

    UPROPERTY(ReplicatedUsing = OnRep_SegmentStates)
    TArray<FCMReplicatedSegmentState> ReplicatedSegmentStates;

    UPROPERTY(ReplicatedUsing = OnRep_SegmentHealthStates,
        BlueprintReadOnly, Category = "Chimera|Health",
        meta = (AllowPrivateAccess = "true"))
    // ASC에 넣지 않은 이유: 각 마디가 따로 죽고 슬롯 두 개만 비활성화되어야 하기 때문이다.
    TArray<FCMBodySegmentHealthState> SegmentHealthStates;

    UPROPERTY(Replicated)
    uint32 PressedPartSlotMask = 0;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> ControlMarkerMaterials;

    TArray<int32> AppliedMarkerColorIndices;
    TArray<int32> AppliedMarkerSlotIndices;
    TArray<bool> AppliedMarkerPressedStates;
    bool bHasReceivedSegmentStates = false;
    bool bAllSegmentsDeathNotified = false;
    float ConfiguredSegmentMaxHealth = 0.0f;
    
    TWeakObjectPtr<ACMSpringArmPart> ActiveSpringArmPull;
    FActiveGameplayEffectHandle StaminaRegenEffectHandle;

    // The coordinator reads the existing editor/CSV tuning fields without
    // moving them and invalidating Blueprint defaults.
    friend class UCMLineBodyMovementCoordinator;
};
