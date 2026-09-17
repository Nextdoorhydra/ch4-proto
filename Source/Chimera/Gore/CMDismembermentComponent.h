#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gore/CMDismembermentDefinition.h"

#include "CMDismembermentComponent.generated.h"

class UActorComponent;
class UCMBloodPoolSourceComponent;
class UMaterialInterface;
class UPrimitiveComponent;
class USkeletalMeshComponent;
class UStaticMesh;
class ACMPartActorBase;
struct FHitResult;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCMCorpseRagdollStartedSignature);

/** Owns modular body-part state and the authoritative transition to a corpse. */
UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
class CHIMERA_API UCMDismembermentComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMDismembermentComponent();

    virtual void BeginPlay() override;

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    /** Configures every modular part to copy the owner's Character mesh pose. */
    UFUNCTION(BlueprintCallable, Category = "Chimera|Dismemberment")
    bool ConfigureLeaderPose();

    /** Server-authored, idempotent transition from a living Character to ragdoll. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Dismemberment")
    bool EnterCorpseRagdoll();

    /** Severs a body part as a temporary, non-collectible visual ragdoll. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Dismemberment")
    bool SeverBodyPart(
        ECMBodyPart BodyPart,
        FVector HitLocation,
        FVector Impulse
    );

    /** Severs a body part as a persistent, collectible gameplay reward. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Dismemberment")
    bool SeverBodyPartWithReward(
        ECMBodyPart BodyPart,
        FVector HitLocation,
        FVector Impulse,
        TSubclassOf<ACMPartActorBase> PartClass
    );

    /** Removes a body part without spawning a cosmetic or collectible actor. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Dismemberment")
    bool ConsumeBodyPart(
        ECMBodyPart BodyPart,
        FVector HitLocation
    );

    UFUNCTION(BlueprintPure, Category = "Chimera|Dismemberment")
    bool IsCorpseRagdoll() const
    {
        return bCorpseRagdoll;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Dismemberment")
    ECMBodyPartState GetPartState(ECMBodyPart BodyPart) const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Dismemberment")
    ECMBodyPart ResolveBodyPartFromComponent(
        const UActorComponent* Component
    ) const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Dismemberment")
    int32 GetConfiguredPartCount() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Dismemberment")
    AActor* GetDetachedPartActor(ECMBodyPart BodyPart) const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Dismemberment")
    int32 GetSpawnedFleshChunkCount() const;

    void DestroySpawnedDismembermentActors();

    UFUNCTION(BlueprintPure, Category = "Chimera|Dismemberment")
    int32 GetBroadcastBloodBurstCount() const
    {
        return BroadcastBloodBurstCount;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Dismemberment")
    int32 GetSpawnedBloodDecalCount() const
    {
        return SpawnedBloodDecalCount;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Dismemberment")
    bool IsCorpseBloodPoolActive() const;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Dismemberment")
    FCMCorpseRagdollStartedSignature OnCorpseRagdollStarted;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Dismemberment")
    TObjectPtr<UCMDismembermentDefinition> Definition;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Dismemberment")
    bool bConfigureLeaderPoseOnBeginPlay = true;

    /** Keeps the legacy six-part fallback; Sacrifice characters disable it. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Dismemberment")
    bool bIncludeTorsoInFallbackDefinition = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Dismemberment|Ragdoll")
    FName RagdollCollisionProfileName = TEXT("Ragdoll");

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Dismemberment|Gore")
    FName BloodDefinitionId = TEXT("Human.Red");

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Dismemberment|Gore",
        meta = (ClampMin = "0.0"))
    float BloodBurstAmount = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Dismemberment|Gore",
        meta = (ClampMin = "0"))
    int32 FleshChunkCount = 4;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Dismemberment|Gore")
    TSoftObjectPtr<UStaticMesh> FleshChunkMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Dismemberment|Gore")
    TSoftObjectPtr<UMaterialInterface> FleshChunkMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Dismemberment|Gore",
        meta = (ClampMin = "0.0"))
    float FleshChunkImpulse = 800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Dismemberment|Gore",
        meta = (ClampMin = "0.0"))
    float SpawnedGoreLifeSpan = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Dismemberment|Blood Trail")
    bool bSpawnBloodDecals = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Dismemberment|Blood Trail",
        meta = (ClampMin = "0.0"))
    float GroundBloodTraceDistance = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Dismemberment|Blood Pool")
    bool bSpawnCorpseBloodPool = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Dismemberment|Blood Pool",
        meta = (ClampMin = "0.0"))
    float CorpseBloodPoolAmount = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Dismemberment|Blood Pool",
        meta = (ClampMin = "0.0"))
    float CorpseBloodPoolGrowthDuration = 6.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Dismemberment|Blood Pool")
    float CorpseBloodPoolLifetime = 45.0f;

private:
    UFUNCTION()
    void OnRep_CorpseRagdoll();

    void InitializePartStates();
    void ApplyCorpseRagdoll();
    bool StartCorpseBloodPool();
    bool SeverBodyPartInternal(
        ECMBodyPart BodyPart,
        FVector HitLocation,
        FVector Impulse,
        TSubclassOf<ACMPartActorBase> RewardPartClass,
        bool bSpawnDetachedPart
    );

    const TArray<FCMDismembermentPartDefinition>&
        GetEffectivePartDefinitions() const;

    USkeletalMeshComponent* FindPartMesh(FName ComponentName) const;
    void SpawnFleshChunks(const FVector& Location, const FVector& Direction);
    void BroadcastBloodBurst(const FVector& Location, const FVector& Direction);
    bool TrySpawnGroundBloodDecal(
        const FVector& Location,
        AActor* IgnoredActor
    );
    bool SpawnBloodDecalAtSurface(
        const FVector& SurfaceLocation,
        const FVector& SurfaceNormal,
        AActor* IgnoredActor
    );

    UPROPERTY(Transient)
    TArray<FCMDismembermentPartDefinition> FallbackParts;

    UPROPERTY(Transient)
    TMap<ECMBodyPart, ECMBodyPartState> PartStates;

    UPROPERTY(Transient)
    TMap<ECMBodyPart, TObjectPtr<AActor>> DetachedPartActors;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AActor>> SpawnedFleshChunks;

    UPROPERTY(Transient)
    int32 BroadcastBloodBurstCount = 0;

    UPROPERTY(Transient)
    int32 SpawnedBloodDecalCount = 0;

    UPROPERTY(Transient)
    TObjectPtr<UCMBloodPoolSourceComponent> CorpseBloodPoolComponent;

    UFUNCTION()
    void OnRep_SeveredPartMask();

    void ApplySeveredPartMask();

    UPROPERTY(ReplicatedUsing = OnRep_SeveredPartMask)
    uint8 SeveredPartMask = 0;

    UPROPERTY(ReplicatedUsing = OnRep_CorpseRagdoll)
    bool bCorpseRagdoll = false;
};
