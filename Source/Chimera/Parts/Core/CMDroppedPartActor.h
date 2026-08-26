#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gore/CMDismembermentDefinition.h"

#include "CMDroppedPartActor.generated.h"

class ACMPartActorBase;
class UPhysicsAsset;
class USkeletalMesh;
class USkeletalMeshComponent;

/** Physical world pickup created by severing a harvestable body part. */
UCLASS(Blueprintable)
class CHIMERA_API ACMDroppedPartActor : public AActor
{
    GENERATED_BODY()

public:
    ACMDroppedPartActor();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    void InitializeDroppedPart(
        ECMBodyPart InBodyPart,
        TSubclassOf<ACMPartActorBase> InUsablePartClass,
        USkeletalMesh* InMesh,
        UPhysicsAsset* InPhysicsAsset,
        FName CollisionProfile,
        FVector Impulse
    );

    /** Converts this pickup into one usable Part and consumes the pickup. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Part Drop")
    ACMPartActorBase* CreateUsablePart(
        const FTransform& SpawnTransform,
        AActor* NewOwner
    );

    UFUNCTION(BlueprintPure, Category = "Chimera|Part Drop")
    ECMBodyPart GetBodyPart() const { return BodyPart; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Part Drop")
    USkeletalMeshComponent* GetPartMesh() const { return PartMesh; }

private:
    UFUNCTION()
    void OnRep_VisualDefinition();

    void ApplyVisualDefinition();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
        meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USkeletalMeshComponent> PartMesh;

    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Part Drop",
        meta = (AllowPrivateAccess = "true"))
    ECMBodyPart BodyPart = ECMBodyPart::None;

    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Part Drop",
        meta = (AllowPrivateAccess = "true"))
    TSubclassOf<ACMPartActorBase> UsablePartClass;

    UPROPERTY(ReplicatedUsing = OnRep_VisualDefinition)
    TObjectPtr<USkeletalMesh> DroppedMesh;

    UPROPERTY(ReplicatedUsing = OnRep_VisualDefinition)
    TObjectPtr<UPhysicsAsset> DroppedPhysicsAsset;

    UPROPERTY(ReplicatedUsing = OnRep_VisualDefinition)
    FName DroppedCollisionProfile = TEXT("Ragdoll");

    bool bConsumed = false;
};
