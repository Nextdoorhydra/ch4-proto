#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/CMControlTypes.h"

#include "CMTentacleSegmentActor.generated.h"

class ACMDroppedPartActor;
class ACMChimera;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class UPrimitiveComponent;
class USceneComponent;
class USkeletalMeshComponent;
class USphereComponent;
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
    void UpdateVisual(float DeltaTime);
    void EnsureVisualComponents();
    void DestroyVisualComponents();
    USkeletalMeshComponent* ResolveTargetPartMesh(AActor* Target) const;
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

    FCMPartSlotAddress PendingPartSlot;
    FTransform PullStartTransform;
    FVector LastVisualTargetLocation = FVector::ZeroVector;
    float PullElapsedSeconds = 0.0f;
    float VisualAlpha = 0.0f;
    bool bSegmentActive = true;
};
