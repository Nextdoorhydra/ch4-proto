#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CMGoreResponseComponent.generated.h"

class UCMBloodPoolSourceComponent;
class UMaterialInterface;
class UStaticMesh;

/** Connects a damageable Chimera actor to the shared CMGore presentation. */
UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
class CHIMERA_API UCMGoreResponseComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMGoreResponseComponent();

    /** Plays the configured hit effect and places one decal below the hit. */
    UFUNCTION(BlueprintCallable, Category = "Chimera|Gore")
    void SpawnHitEffects(
        FVector HitLocation,
        FVector SurfaceNormal,
        FVector BloodDirection,
        float Intensity = 1.0f
    );

    /** Adds the shared burst, flesh chunks, ground decal, and blood pool. */
    UFUNCTION(BlueprintCallable, Category = "Chimera|Gore")
    void SpawnDestructionEffects(
        FVector Location,
        FVector Direction
    );

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Gore")
    FName BloodDefinitionId = TEXT("Human.Red");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Gore",
        meta = (ClampMin = "0.0"))
    float BloodBurstAmount = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Gore",
        meta = (ClampMin = "0"))
    int32 FleshChunkCount = 4;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Gore")
    TSoftObjectPtr<UStaticMesh> FleshChunkMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Gore")
    TSoftObjectPtr<UMaterialInterface> FleshChunkMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Gore",
        meta = (ClampMin = "0.0"))
    float FleshChunkImpulse = 800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Gore",
        meta = (ClampMin = "0.0"))
    float SpawnedGoreLifeSpan = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Gore|Ground Decal")
    bool bSpawnGroundBloodDecal = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Gore|Ground Decal",
        meta = (ClampMin = "0.0"))
    float GroundBloodTraceDistance = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Gore|Blood Pool")
    bool bSpawnBloodPoolOnDestruction = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Gore|Blood Pool",
        meta = (ClampMin = "0.0"))
    float BloodPoolAmount = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Gore|Blood Pool",
        meta = (ClampMin = "0.0"))
    float BloodPoolGrowthDuration = 6.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Gore|Blood Pool")
    float BloodPoolLifetime = 45.0f;

private:
    void BroadcastBloodImpact(
        const FVector& Location,
        const FVector& SurfaceNormal,
        const FVector& Direction,
        float Intensity
    );
    void BroadcastBloodBurst(
        const FVector& Location,
        const FVector& Direction
    );
    void SpawnFleshChunks(
        const FVector& Location,
        const FVector& Direction
    );
    bool SpawnGroundBloodDecal(const FVector& Location);
    bool StartBloodPool(const FVector& Location);

    UPROPERTY(Transient)
    TArray<TObjectPtr<AActor>> SpawnedFleshChunks;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UCMBloodPoolSourceComponent>> BloodPoolComponents;
};
