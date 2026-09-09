#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CMChimeraBodySegmentActor.generated.h"

class ACMChimera;
class ACMTentacleSegmentActor;
class UChildActorComponent;
class UCMChimeraIdleTentacleComponent;
class UCMChimeraVisualDefinition;
class UPrimitiveComponent;
class USceneComponent;
class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class ECMChimeraSegmentVisualRole : uint8
{
    Head,
    Body,
    Tail
};

namespace CMChimeraVisual
{
    CHIMERA_API ECMChimeraSegmentVisualRole ResolveSegmentVisualRole(
        int32 SegmentIndex,
        int32 ActiveSegmentCount);
}

/**
 * Stable authoring and presentation boundary for one physical Chimera segment.
 * Gameplay physics remains on ACMChimera's BodyMesh_n UBoxComponent.
 */
UCLASS(Blueprintable)
class CHIMERA_API ACMChimeraBodySegmentActor : public AActor
{
    GENERATED_BODY()

public:
    ACMChimeraBodySegmentActor();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    void SetTentacleActorClass(
        TSubclassOf<ACMTentacleSegmentActor> InTentacleActorClass);

    void InitializeForSegment(
        ACMChimera* InChimera,
        int32 InSegmentIndex,
        UPrimitiveComponent* InBodySegment);

    void SetSegmentPresentation(
        bool bInActive,
        ECMChimeraSegmentVisualRole InVisualRole);

    UFUNCTION(BlueprintPure, Category = "Chimera|Body Segment")
    int32 GetSegmentIndex() const { return SegmentIndex; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Body Segment")
    bool IsSegmentActive() const { return bSegmentActive; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Body Segment")
    ECMChimeraSegmentVisualRole GetVisualRole() const { return VisualRole; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Body Segment")
    ACMTentacleSegmentActor* GetTentacleActor() const;

    /** Rebinds IdleTentacles to this segment's GooBody static mesh. */
    void RefreshIdleTentacleSource();

protected:
    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USkeletalMeshComponent> BodyVisual;

    /** Author-visible, stable TentacleActor slot owned by this segment. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UChildActorComponent> TentacleActor;

    /** Local cosmetic tentacles sampled from the segment's source mesh. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UCMChimeraIdleTentacleComponent> IdleTentacles;

    /** Head/Body/Tail meshes and their import-validation contract. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Body Segment")
    TObjectPtr<UCMChimeraVisualDefinition> VisualDefinition;

    UFUNCTION(BlueprintImplementableEvent,
        Category = "Chimera|Body Segment",
        meta = (DisplayName = "Apply Segment Visual Role"))
    void K2_ApplySegmentVisualRole(ECMChimeraSegmentVisualRole InVisualRole);

    UFUNCTION(BlueprintImplementableEvent,
        Category = "Chimera|Body Segment",
        meta = (DisplayName = "Set Segment Visual Active"))
    void K2_SetSegmentVisualActive(bool bInActive);

private:
    void ApplyVisualPreset();

    UFUNCTION()
    void OnRep_PresentationState();

    UPROPERTY(Transient)
    TObjectPtr<ACMChimera> ChimeraOwner;

    UPROPERTY(Transient)
    TObjectPtr<UPrimitiveComponent> BodySegment;

    UPROPERTY(ReplicatedUsing = OnRep_PresentationState)
    int32 SegmentIndex = INDEX_NONE;

    UPROPERTY(ReplicatedUsing = OnRep_PresentationState)
    bool bSegmentActive = false;

    bool bHasPresentationState = false;

    UPROPERTY(ReplicatedUsing = OnRep_PresentationState)
    ECMChimeraSegmentVisualRole VisualRole =
        ECMChimeraSegmentVisualRole::Body;
};
