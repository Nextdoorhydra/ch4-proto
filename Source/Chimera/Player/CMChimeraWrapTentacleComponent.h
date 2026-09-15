#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"

#include "CMChimeraWrapTentacleComponent.generated.h"

class UMaterialInterface;
class UMeshComponent;
class UProceduralMeshComponent;
class USkeletalMeshComponent;
class USplineComponent;
class UStaticMeshComponent;

namespace CMChimeraWrapTentacle
{
    CHIMERA_API float AdvanceAlpha(
        float CurrentAlpha,
        bool bExtending,
        float DeltaTime,
        float ExtensionDuration,
        float RetractionDuration);

    CHIMERA_API float ResolvePointRevealAlpha(
        float ExtensionAlpha,
        int32 PointIndex,
        int32 PointCount);

    CHIMERA_API bool IsWithinActivationDistance(
        const FVector& SourcePosition,
        const FBoxSphereBounds& TargetBounds,
        float ActivationDistance);
}

USTRUCT()
struct FCMChimeraWrapSurfaceAnchor
{
    GENERATED_BODY()

    FVector TargetLocalPosition = FVector::ZeroVector;
    FVector TargetLocalNormal = FVector::UpVector;
    FName BoneName = NAME_None;
    FVector BoneLocalPosition = FVector::ZeroVector;
    FVector BoneLocalNormal = FVector::UpVector;
};

USTRUCT()
struct FCMChimeraWrapTentacleRuntime
{
    GENERATED_BODY()

    UPROPERTY(Transient)
    TObjectPtr<USplineComponent> Spline;

    UPROPERTY(Transient)
    TObjectPtr<UProceduralMeshComponent> TubeMesh;

    TArray<FCMChimeraWrapSurfaceAnchor> TargetAnchors;
    float NoisePhase = 0.0f;
};

/**
 * Local-only pooled spline tentacles that grow from a Chimera segment and
 * follow distance-ordered surface anchors on a target mesh.
 */
UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
class CHIMERA_API UCMChimeraWrapTentacleComponent
    : public USceneComponent
{
    GENERATED_BODY()

public:
    UCMChimeraWrapTentacleComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    void ConfigureSource(UMeshComponent* InSourceMesh, int32 SegmentIndex);
    void SetEffectActive(bool bInActive);

    UFUNCTION(BlueprintCallable, Category = "Chimera|Wrap Tentacle")
    void SetTargetMesh(UMeshComponent* InTargetMesh);

    UFUNCTION(BlueprintPure, Category = "Chimera|Wrap Tentacle")
    UMeshComponent* GetTargetMesh() const { return TargetMesh; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Wrap Tentacle")
    int32 GetActiveTentacleCount() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Wrap Tentacle")
    float GetExtensionAlpha() const { return ExtensionAlpha; }

protected:
    virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Wrap Tentacle|Activation",
        meta = (ClampMin = "1.0"))
    float ActivationDistance = 450.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Wrap Tentacle|Activation",
        meta = (ClampMin = "0.01"))
    float ExtensionDuration = 1.2f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Wrap Tentacle|Activation",
        meta = (ClampMin = "0.01"))
    float RetractionDuration = 0.8f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Wrap Tentacle|Sampling",
        meta = (ClampMin = "0"))
    int32 TargetLODIndex = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Wrap Tentacle|Sampling",
        meta = (ClampMin = "0.0"))
    float MinimumTargetAnchorDistance = 2.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Wrap Tentacle|Sampling",
        meta = (ClampMin = "0.1"))
    float SamplingRetryInterval = 0.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Wrap Tentacle|Shape",
        meta = (ClampMin = "1"))
    int32 TentacleCount = 3;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Wrap Tentacle|Shape",
        meta = (ClampMin = "3"))
    int32 SplinePointCount = 32;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Wrap Tentacle|Shape",
        meta = (ClampMin = "0.01"))
    float TentacleWidth = 0.25f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Wrap Tentacle|Shape",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Entanglement = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Wrap Tentacle|Shape",
        meta = (ClampMin = "0.0"))
    float EntanglementAmplitude = 30.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Wrap Tentacle|Shape",
        meta = (ClampMin = "0.0"))
    float EntanglementCycles = 1.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Wrap Tentacle|Shape",
        meta = (ClampMin = "0.0"))
    float SurfaceOffset = 2.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Wrap Tentacle|Rendering")
    TObjectPtr<UMaterialInterface> TentacleMaterial;

private:
    friend class FCMChimeraWrapTentacleMathTest;
    friend class FCMChimeraWrapTentacleRuntimeTest;
    friend class FCMTentacleBlueprintIntegrationTest;

    bool BuildSurfaceCandidates();
    bool BuildSkeletalSurfaceCandidates(
        USkeletalMeshComponent& SkeletalTarget);
    bool BuildStaticSurfaceCandidates(UStaticMeshComponent& StaticTarget);
    void AddSurfaceCandidate(
        const FVector& TargetLocalPosition,
        const FVector& TargetLocalNormal,
        USkeletalMeshComponent* SkeletalTarget);
    bool AssignDistanceOrderedPaths();
    void EnsurePool();
    void DestroyPool();
    void HidePool();
    void UpdateTentacleMeshes();
    void UpdateTubeMesh(FCMChimeraWrapTentacleRuntime& Runtime);
    FVector ResolveAnchorWorldPosition(
        const FCMChimeraWrapSurfaceAnchor& Anchor) const;
    FVector ResolveAnchorWorldNormal(
        const FCMChimeraWrapSurfaceAnchor& Anchor) const;
    FVector GetSourceWorldPosition() const;
    bool IsTargetInRange() const;
    void ResetTargetState();
    void RefreshTickEnabled();

    UPROPERTY(Transient)
    TObjectPtr<UMeshComponent> SourceMesh;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Wrap Tentacle",
        meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UMeshComponent> TargetMesh;

    UPROPERTY(Transient)
    TArray<FCMChimeraWrapTentacleRuntime> RuntimeTentacles;

    TArray<FCMChimeraWrapSurfaceAnchor> SurfaceCandidates;
    FRandomStream RandomStream;
    float ExtensionAlpha = 0.0f;
    float ElapsedTime = 0.0f;
    float NextSamplingTime = 0.0f;
    bool bEffectActive = false;
    bool bPathsReady = false;
    bool bLoggedSamplingFailure = false;
};
