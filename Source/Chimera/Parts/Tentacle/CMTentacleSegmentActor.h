#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/CMControlTypes.h"

#include "CMTentacleSegmentActor.generated.h"

class ACMDroppedPartActor;
class ACMChimera;
class UAudioComponent;
class UCMPartSlotComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class UProceduralMeshComponent;
class UPrimitiveComponent;
class USceneComponent;
class USkeletalMeshComponent;
class USphereComponent;
class USplineComponent;
class USplineMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ECMTentacleState : uint8
{
    Idle,
    Extended,
    Pulling
};

USTRUCT()
struct FCMMountedHeadTentacleRuntime
{
    GENERATED_BODY()

    UPROPERTY(Transient)
    TObjectPtr<USplineComponent> Spline;

    UPROPERTY(Transient)
    TObjectPtr<UProceduralMeshComponent> TubeMesh;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> TubeMaterial;

    /** Visual mesh moved by the hanging idle. The Actor root stays network-stable. */
    UPROPERTY(Transient)
    TObjectPtr<USkeletalMeshComponent> AnimatedHeadMesh;

    FTransform HeadMeshBaseRelativeTransform = FTransform::Identity;
    bool bHasHeadMeshBaseTransform = false;
    bool bLoggedConnectorFailure = false;
    bool bLoggedConnectorReady = false;
};

/**
 * One runtime-deformed tentacle paired with one Chimera body segment.
 *
 * The server selects the nearest tagged overlap every Tick. Clients only
 * render the replicated target and never participate in attachment decisions.
 */
UCLASS(Blueprintable)
class CHIMERA_API ACMTentacleSegmentActor : public AActor
{
    GENERATED_BODY()

public:
    ACMTentacleSegmentActor();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** Server initialization performed by the owning Chimera. */
    void InitializeForSegment(
        ACMChimera* InChimera,
        int32 InSegmentIndex,
        UPrimitiveComponent* InBodySegment,
        bool bAttachToBodySegment = true);

    /** Enables interaction and rendering without changing this Actor's lifetime. */
    void SetSegmentActive(bool bInActive);

    /** Starts pulling the currently tethered dropped Part into an empty slot. */
    bool TryBeginPartAttachment(
        const FCMPartSlotAddress& PartSlotAddress);

    UFUNCTION(BlueprintPure, Category = "Chimera|Tentacle")
    int32 GetSegmentIndex() const { return SegmentIndex; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Tentacle")
    AActor* GetTetheredActor() const { return TetheredActor; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Tentacle")
    bool HasAttachablePart() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Tentacle")
    bool IsSegmentActive() const { return bSegmentActive; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Tentacle")
    UStaticMeshComponent* GetGooBodyComponent() const { return GooBody; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Tentacle")
    float GetTetheredTentacleWidth() const
    {
        return TetheredTentacleWidth;
    }

    /** Number of visible cosmetic tethers supporting mounted Head Parts. */
    UFUNCTION(BlueprintPure, Category = "Chimera|Tentacle")
    int32 GetMountedHeadTentacleCount() const;

    static const FName TentacleInteractiveActorTag;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> DetectionSphere;

    /** BP_Goo's always-visible base blob. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> GooBody;

    /** BP_Goo's NS_Goo_Up component. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UNiagaraComponent> SourceEffect;

    /** BP_Goo's NS_Goo_Drips component. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UNiagaraComponent> GooDripsEffect;

    /** Offset toward the gap behind the corresponding BodyMesh_n. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle|Placement")
    FVector SourceRelativeOffset = FVector(-60.0f, 0.0f, 0.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle|Detection",
        meta = (ClampMin = "1.0"))
    float DetectionRadius = 420.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle|Detection")
    FName InteractiveActorTag = TentacleInteractiveActorTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle|Visual")
    TArray<TObjectPtr<UStaticMesh>> TentacleMeshVariants;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle|Visual")
    TObjectPtr<UMaterialInterface> TentacleMaterial;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle|Goo Base")
    TObjectPtr<UStaticMesh> GooBodyMesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle|Goo Base")
    TObjectPtr<UMaterialInterface> GooBodyMaterial;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle|Goo Base")
    TObjectPtr<UNiagaraSystem> GooDripsNiagaraSystem;

    /** BP_Goo's NS_Goo_Up system. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle|Goo Base")
    TObjectPtr<UNiagaraSystem> SourceNiagaraSystem;

    /** BP_Spline's NS_Goo_Small_Drips system. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle|Visual")
    TObjectPtr<UNiagaraSystem> TargetNiagaraSystem;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle|Visual")
    FName CollapseMaterialParameter = TEXT("WPO Collapse Lerp");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle|Visual",
        meta = (ClampMin = "0.01"))
    float ExtensionDuration = 0.35f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle|Visual",
        meta = (ClampMin = "0.01"))
    float RetractionDuration = 0.25f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle|Visual",
        meta = (ClampMin = "0.0"))
    float TangentNoise = 80.0f;

    /** Cross-section scale of the tentacle reaching toward a Part. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle|Visual",
        meta = (ClampMin = "0.01"))
    float TetheredTentacleWidth = 1.8f;

    /** Idle-tentacle width used by the persistent mounted-Head tether. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle|Visual",
        meta = (ClampMin = "0.01"))
    float MountedHeadTentacleWidth = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle|Visual",
        meta = (ClampMin = "2"))
    int32 MountedHeadSplinePointCount = 5;

    /** Upward arch that keeps the mounted-Head connector clear of the ground. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle|Visual",
        meta = (ClampMin = "0.0"))
    float MountedHeadTentacleSag = 45.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle|Visual",
        meta = (ClampMin = "0.0"))
    float MountedHeadWaveAmplitude = 8.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle|Visual",
        meta = (ClampMin = "0.0"))
    float MountedHeadWaveSpeed = 2.4f;

    /** Raises the mounted Head above its authored slot before idle movement. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle|Mounted Head Idle",
        meta = (ClampMin = "0.0"))
    float MountedHeadIdleHeightOffset = 80.0f;

    /** Vertical travel of a mounted Head hanging from its idle tentacle. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle|Mounted Head Idle",
        meta = (ClampMin = "0.0"))
    float MountedHeadIdleVerticalAmplitude = 10.0f;

    /** Side-to-side travel of a mounted Head hanging from its idle tentacle. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle|Mounted Head Idle",
        meta = (ClampMin = "0.0"))
    float MountedHeadIdleHorizontalAmplitude = 6.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle|Mounted Head Idle",
        meta = (ClampMin = "0.0"))
    float MountedHeadIdleSpeed = 1.25f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle|Attachment",
        meta = (ClampMin = "0.01"))
    float PullDuration = 0.65f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle|Attachment",
        meta = (ClampMin = "0.0"))
    float AttachmentAcceptanceDistance = 25.0f;

    UPROPERTY(ReplicatedUsing = OnRep_VisualState,
        VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle")
    TObjectPtr<AActor> TetheredActor;

    UPROPERTY(ReplicatedUsing = OnRep_VisualState,
        VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle")
    ECMTentacleState TentacleState = ECMTentacleState::Idle;

    UPROPERTY(ReplicatedUsing = OnRep_VisualState)
    int32 VisualSeed = 0;

    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Tentacle")
    int32 SegmentIndex = INDEX_NONE;

private:
    friend class FCMTentacleBlueprintIntegrationTest;

    void RefreshOverlapTarget();
    void SetTetheredActor(AActor* NewTarget);
    void UpdatePull(float DeltaTime);
    void AbortPull();
    void RefreshPullLoopSound();
    void StopPullLoopSound();
    void UpdateVisual(float DeltaTime);
    void EnsureVisualComponents();
    void DestroyVisualComponents();
    void UpdateMountedHeadTentacles(float DeltaTime);
    void EnsureMountedHeadTentacle(int32 PartSlotIndex);
    void UpdateMountedHeadIdleAnimation(
        FCMMountedHeadTentacleRuntime& Runtime,
        USkeletalMeshComponent& HeadMesh,
        int32 PartSlotIndex);
    void RestoreMountedHeadIdleAnimation(
        FCMMountedHeadTentacleRuntime& Runtime);
    void HideMountedHeadTentacle(int32 PartSlotIndex);
    void DestroyMountedHeadTentacles();
    bool ResolveMountedHeadNeckLocation(
        const UCMPartSlotComponent& PartSlot,
        const USkeletalMeshComponent& HeadMesh,
        FVector& OutWorldLocation,
        FName* OutBoneName = nullptr) const;
    FVector ResolveMountedHeadSourceLocation(
        const FVector& HeadTargetLocation) const;
    USkeletalMeshComponent* ResolveTargetPartMesh(AActor* Target) const;
    FVector ResolveAuthoritativePickupLocation(AActor* Target) const;
    FVector ResolveVisualTargetLocation(AActor* Target) const;

    UFUNCTION()
    void OnRep_VisualState();

    UPROPERTY(Transient)
    TObjectPtr<ACMChimera> ChimeraOwner;

    UPROPERTY(Transient)
    TObjectPtr<USplineMeshComponent> RuntimeSplineMesh;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> RuntimeMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UNiagaraComponent> TargetEffect;

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> PullLoopSoundComponent;

    UPROPERTY(Transient)
    TArray<FCMMountedHeadTentacleRuntime> MountedHeadTentacles;

    FCMPartSlotAddress PendingPartSlot;
    FTransform PullStartTransform;
    FVector LastVisualTargetLocation = FVector::ZeroVector;
    float PullElapsedSeconds = 0.0f;
    float VisualAlpha = 0.0f;
    float MountedHeadTentacleElapsedSeconds = 0.0f;
    bool bSegmentActive = true;
};
