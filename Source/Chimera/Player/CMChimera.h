#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Pawn.h"

#include "CMControlTypes.h"
#include "CMChimera.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UPhysicsConstraintComponent;
class USpringArmComponent;
class UCameraComponent;
class UMaterialInstanceDynamic;
class UTextRenderComponent;
class UAbilitySystemComponent;
class UCMChimeraAttributeSet;
class UCMLineBodyMovementCoordinator;
class UCMPartSlotComponent;
class UDataTable;
class UPhysicalMaterial;
class ACMPlayerState;
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
        ACMPlayerState* ContributingPlayerState
    );

    void SetPartSlotPressed(
        const FCMPartSlotAddress& PartSlotAddress,
        bool bPressed
    );

    void ClearPressedControlParts();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Health")
    void ApplyDamageToSegment(int32 SegmentIndex, float Damage);

    UFUNCTION(BlueprintPure, Category = "Chimera|Health")
    bool IsSegmentAlive(int32 SegmentIndex) const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Health")
    TArray<FCMBodySegmentHealthState> GetSegmentHealthStates() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Health")
    bool AreAllSegmentsDead() const;

    /** Keeps the active body count equal to the 4~8 participating players. */
    void SetActiveSegmentCountForPlayers(int32 PlayerCount);

    UFUNCTION(BlueprintPure, Category = "Chimera")
    int32 GetActiveSegmentCount() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Part Slots")
    UCMPartSlotComponent* GetPartSlotComponent(
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

#if !UE_BUILD_SHIPPING
    /** Attaches random Head/Arm/Leg diagnostic Parts to empty active slots. */
    void SpawnRandomDebugParts();

    /** Detaches and destroys only diagnostic Part Actors. */
    void ClearRandomDebugParts();

    /** Lets the diagnostic Leg GA exercise the current LineBody movement. */
    void ActivateDebugLegPart(
        const FCMPartSlotAddress& PartSlotAddress,
        ACMPlayerState* ContributingPlayerState
    );
#endif

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
        Category = "Chimera")
    TArray<TObjectPtr<USceneComponent>> LeftFootPoints;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient,
        Category = "Chimera")
    TArray<TObjectPtr<USceneComponent>> RightFootPoints;

    /** Flattened as SegmentIndex * 4 + PartSlotIndex. */
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

    UPROPERTY(EditAnywhere, Category = "Leg")
    float LegImpulse = 5000.0f;

    // Temporary LineBody action cost. Later, each attached Part can supply
    // its own cost while the shared ASC continues to pay it the same way.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leg|Stamina",
        meta = (ClampMin = "0.0"))
    float LegStaminaCost = 10.0f;

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
    void ApplyStaminaCost(float Cost);
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

    // The coordinator reads the existing editor/CSV tuning fields without
    // moving them and invalidating Blueprint defaults.
    friend class UCMLineBodyMovementCoordinator;
};
