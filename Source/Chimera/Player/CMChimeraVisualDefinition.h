#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Player/CMChimeraBodySegmentActor.h"

#include "CMChimeraVisualDefinition.generated.h"

class UAnimInstance;
class USkeletalMesh;

/** One visual preset applied to the stable BodyVisual component. */
USTRUCT(BlueprintType)
struct CHIMERA_API FCMChimeraSegmentVisualPreset
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
    TObjectPtr<USkeletalMesh> Mesh;

    /** Optional until the segment AnimBP phase is complete. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
    TSubclassOf<UAnimInstance> AnimClass;

    /** Authoring correction only. Scale must remain one. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
    FTransform RelativeTransform = FTransform::Identity;
};

/**
 * Import contract and runtime visual presets for the three Chimera segment
 * roles. Content Browser Data Validation checks the imported meshes against
 * the shared skeleton, death-rig, and surface-sampling contracts.
 */
UCLASS(BlueprintType)
class CHIMERA_API UCMChimeraVisualDefinition : public UDataAsset
{
    GENERATED_BODY()

public:
    UCMChimeraVisualDefinition();

    const FCMChimeraSegmentVisualPreset& GetPreset(
        ECMChimeraSegmentVisualRole Role) const;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
    FCMChimeraSegmentVisualPreset Head;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
    FCMChimeraSegmentVisualPreset Body;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
    FCMChimeraSegmentVisualPreset Tail;

    /** Exact names required on all three meshes. */
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Validation")
    TArray<FName> RequiredBones;

    /** Exact Skeletal Mesh Sampling Region names required on all meshes. */
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Validation")
    TArray<FName> RequiredSamplingRegions;

    /** Physics bodies needed to expose the core and detach three shells. */
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Validation")
    TArray<FName> RequiredPhysicsBodies;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(
        FDataValidationContext& Context) const override;
#endif
};
