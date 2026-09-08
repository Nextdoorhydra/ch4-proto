#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CMChimeraTrailComponent.generated.h"

class UDecalComponent;
class UAudioComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTexture;

namespace CMChimeraTrail
{
    struct FStampPlan
    {
        int32 StampCount = 0;
        float FirstStampDistance = 0.0f;
        float CarriedDistance = 0.0f;
    };

    CHIMERA_API FStampPlan BuildStampPlan(
        float CarriedDistance,
        float MovementDistance,
        float StampSpacing,
        int32 MaxStamps);

    CHIMERA_API bool IsTeleport(
        float MovementDistance,
        float TeleportDistance);

    CHIMERA_API FQuat BuildDecalRotation(
        const FVector& SurfaceNormal,
        const FVector& TangentDirection);
}

/**
 * Local-only overlapping decal stamps that paint the Chimera's ground path.
 */
UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
class CHIMERA_API UCMChimeraTrailComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMChimeraTrailComponent();

    UFUNCTION(BlueprintCallable, Category = "Chimera|Trail")
    void SetEffectActive(bool bInActive);

    UFUNCTION(BlueprintPure, Category = "Chimera|Trail")
    int32 GetActiveStampCount() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Trail")
    int32 GetPoolSize() const { return DecalPool.Num(); }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Trail")
    bool bEffectActive = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Trail")
    TObjectPtr<UMaterialInterface> TrailMaterial;

    /** Applied to both floor and wall alpha slots of the decal material. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Trail")
    TObjectPtr<UTexture> BrushTexture;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Trail",
        meta = (ClampMin = "1.0"))
    float StampSpacing = 45.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Trail",
        meta = (ClampMin = "1.0"))
    float TrailWidth = 37.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Trail",
        meta = (ClampMin = "1.0"))
    float StampLength = 37.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Trail",
        meta = (ClampMin = "0.0"))
    float TrailOpacity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Trail",
        meta = (ClampMin = "0.1"))
    float DecalDepth = 16.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Trail",
        meta = (ClampMin = "0.0"))
    float Lifetime = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Trail",
        meta = (ClampMin = "0.0"))
    float FadeDuration = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Trail",
        meta = (ClampMin = "1"))
    int32 PoolCapacity = 8192;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Trail",
        meta = (ClampMin = "1"))
    int32 MaxStampsPerFrame = 64;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Trail",
        meta = (ClampMin = "1.0"))
    float TeleportDistance = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Trail",
        meta = (ClampMin = "0.0"))
    float TraceHeight = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Trail",
        meta = (ClampMin = "0.0"))
    float TraceDepth = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Trail",
        meta = (ClampMin = "0.0"))
    float SurfaceOffset = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Trail")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Trail|Sound",
        meta = (ClampMin = "0.0", Units = "cm/s"))
    float DragSoundMinimumSpeed = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Trail|Sound",
        meta = (ClampMin = "0.0", Units = "s"))
    float DragSoundStopDelay = 0.15f;

private:
    friend class FCMChimeraTrailRuntimeTest;

    struct FSourceState
    {
        FVector PreviousLocation = FVector::ZeroVector;
        float CarriedDistance = 0.0f;
        bool bHasSample = false;
    };

    void InitializePool();
    UMaterialInstanceDynamic* GetOrCreateStampMaterial(int32 StampIndex);
    void DeactivateStamp(int32 StampIndex);
    void UpdateExpiredStamps(float WorldTime);
    void ResetMovementSamples();
    int32 GetSourceCount() const;
    bool GetSourceLocation(int32 SourceIndex, FVector& OutLocation) const;
    int32 UpdateSourceTrail(
        int32 SourceIndex,
        const FVector& CurrentLocation,
        float WorldTime,
        int32 StampBudget);
    bool TryPlaceStamp(
        const FVector& SampleLocation,
        const FVector& MovementDirection,
        float WorldTime);
    void UpdateDragLoopSound(bool bAnySourceMoving, float WorldTime);
    void StopDragLoopSound();

    UPROPERTY(Transient)
    TArray<TObjectPtr<UDecalComponent>> DecalPool;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> DecalMaterials;

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> DragLoopSoundComponent;

    TArray<float> ExpirationTimes;
    TArray<FSourceState> SourceStates;
    int32 NextPoolIndex = 0;
    float LastDragMovementTime = -1.0f;
};
