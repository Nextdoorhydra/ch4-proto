#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"

#include "CMChimeraIdleTentacleComponent.generated.h"

class UMaterialInterface;
class UMeshComponent;
class USkeletalMeshComponent;
class USplineComponent;
class USplineMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

namespace CMChimeraIdleTentacle
{
    CHIMERA_API float ResolveLifecycleLengthAlpha(
        float Age,
        float GrowthDuration,
        float IdleDuration,
        float RetractionDuration);

    CHIMERA_API float ResolveRetractionAlpha(
        float Age,
        float IdleDuration,
        float RetractionDuration);

    CHIMERA_API FVector EvaluateSplinePoint(
        const FVector& Anchor,
        const FVector& Normal,
        const FVector& AxisX,
        const FVector& AxisY,
        float NormalizedDistance,
        float Length,
        float WaveAmplitude,
        float WavePhase,
        float WaveCycles,
        float RetractionAlpha);

    CHIMERA_API bool IsSpacedFromActiveAnchors(
        const FVector& Candidate,
        TConstArrayView<FVector> ActiveAnchors,
        float MinimumDistance);
}

USTRUCT()
struct FCMChimeraIdleTentacleRuntime
{
    GENERATED_BODY()

    UPROPERTY(Transient)
    TObjectPtr<USplineComponent> Spline;

    UPROPERTY(Transient)
    TArray<TObjectPtr<USplineMeshComponent>> MeshSegments;

    FVector Anchor = FVector::ZeroVector;
    FVector Normal = FVector::UpVector;
    FVector AxisX = FVector::ForwardVector;
    FVector AxisY = FVector::RightVector;
    float Age = 0.0f;
    float RespawnDelay = 0.0f;
    float WavePhase = 0.0f;
    bool bActive = false;
};

/** Local-only pooled idle tentacles sampled from a skeletal or static mesh. */
UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
class CHIMERA_API UCMChimeraIdleTentacleComponent
    : public USceneComponent
{
    GENERATED_BODY()

public:
    UCMChimeraIdleTentacleComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    void ConfigureSource(
        UMeshComponent* InSourceMesh,
        int32 SegmentIndex);
    void NotifySourceMeshChanged();
    void SetEffectActive(bool bInActive);

    UFUNCTION(BlueprintPure, Category = "Chimera|Idle Tentacle")
    bool IsEffectActive() const { return bEffectActive; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Idle Tentacle")
    int32 GetActiveTentacleCount() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Idle Tentacle")
    UMeshComponent* GetSourceMeshComponent() const { return SourceMesh; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Idle Tentacle")
    int32 GetSurfaceCandidateCount() const
    {
        return SurfaceCandidates.Num();
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Idle Tentacle")
    float GetTentacleLength() const { return TentacleLength; }

protected:
    virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Idle Tentacle|Sampling")
    FName SamplingRegionName = TEXT("IdleTentacleTop");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Idle Tentacle|Sampling",
        meta = (ClampMin = "1"))
    int32 TentacleCount = 4;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Idle Tentacle|Sampling",
        meta = (ClampMin = "0.0"))
    float MinimumSampleDistance = 20.0f;

    /** LOD read directly when the source is a Static Mesh. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Idle Tentacle|Static Mesh Sampling",
        meta = (ClampMin = "0"))
    int32 StaticMeshLODIndex = 0;

    /** Normalized local bounds height where eligible upper triangles begin. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Idle Tentacle|Static Mesh Sampling",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float StaticMeshMinimumRelativeHeight = 0.5f;

    /** Minimum local +Z dot product for an eligible outward normal. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Idle Tentacle|Static Mesh Sampling",
        meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float StaticMeshMinimumUpNormal = 0.1f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Idle Tentacle|Shape",
        meta = (ClampMin = "2"))
    int32 SplinePointCount = 5;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Idle Tentacle|Shape",
        meta = (ClampMin = "1.0"))
    float TentacleLength = 90.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Idle Tentacle|Shape",
        meta = (ClampMin = "0.0"))
    float WaveAmplitude = 22.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Idle Tentacle|Shape",
        meta = (ClampMin = "0.0"))
    float WaveCycles = 1.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Idle Tentacle|Shape",
        meta = (ClampMin = "0.01"))
    float WaveSpeed = 2.4f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Idle Tentacle|Shape",
        meta = (ClampMin = "0.01"))
    float TentacleWidth = 0.6f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Idle Tentacle|Lifecycle",
        meta = (ClampMin = "0.01"))
    float GrowthDuration = 0.65f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Idle Tentacle|Lifecycle",
        meta = (ClampMin = "0.01"))
    float IdleDuration = 1.8f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Idle Tentacle|Lifecycle",
        meta = (ClampMin = "0.01"))
    float RetractionDuration = 0.45f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Idle Tentacle|Lifecycle",
        meta = (ClampMin = "0.0"))
    FVector2D RespawnDelayRange = FVector2D(0.25f, 0.8f);

    /** Static mesh authored along local +X, reused for every spline span. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Idle Tentacle|Rendering")
    TObjectPtr<UStaticMesh> TentacleMesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Idle Tentacle|Rendering")
    TObjectPtr<UMaterialInterface> TentacleMaterial;

private:
    struct FSurfaceCandidate
    {
        FVector Position = FVector::ZeroVector;
        FVector Normal = FVector::UpVector;
    };

    bool BuildSurfaceCandidates();
    bool BuildSkeletalSurfaceCandidates(
        USkeletalMeshComponent& SkeletalSource);
    bool BuildStaticSurfaceCandidates(UStaticMeshComponent& StaticSource);
    bool HasUsableSourceMesh() const;
    void AddSurfaceCandidate(
        const FVector& SourceLocalPosition,
        const FVector& SourceLocalNormal);
    void EnsurePool();
    void DestroyPool();
    bool TryActivateTentacle(FCMChimeraIdleTentacleRuntime& Runtime);
    void DeactivateTentacle(FCMChimeraIdleTentacleRuntime& Runtime);
    void UpdateTentacle(
        FCMChimeraIdleTentacleRuntime& Runtime,
        float DeltaTime);

    UPROPERTY(Transient)
    TObjectPtr<UMeshComponent> SourceMesh;

    UPROPERTY(Transient)
    TArray<FCMChimeraIdleTentacleRuntime> RuntimeTentacles;

    TArray<FSurfaceCandidate> SurfaceCandidates;
    FRandomStream RandomStream;
    float SurfaceBuildRetryTime = 0.0f;
    int32 SurfaceBuildAttempts = 0;
    bool bEffectActive = false;
};
