#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gore/CMDismembermentDefinition.h"
#include "Player/CMControlTypes.h"

#include "CMDroppedPartActor.generated.h"

class ACMPartActorBase;
class ACMChimera;
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

    /** Server-only reservation used to resolve simultaneous slot presses. */
    bool TryReserveForTentacle(AActor* Requester);
    void ReleaseTentacleReservation(AActor* Requester);
    bool IsReservedForTentacle(const AActor* Requester = nullptr) const;
    bool IsReservedByTentacle(const AActor* Requester) const;

    /** Temporarily hands movement to one tentacle without consuming the pickup. */
    bool BeginTentaclePull(AActor* Requester);
    void EndTentaclePull(AActor* Requester);

    /** Spawns the usable Part, attaches it, then consumes this pickup on success. */
    bool ConsumeIntoPartSlot(
        ACMChimera* Chimera,
        const FCMPartSlotAddress& PartSlotAddress);

private:
    UFUNCTION()
    void OnRep_VisualDefinition();

    UFUNCTION()
    void OnRep_TentaclePulled();

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

    UPROPERTY(ReplicatedUsing = OnRep_TentaclePulled)
    bool bTentaclePulled = false;

    bool bConsumed = false;
    TWeakObjectPtr<AActor> TentacleReservationOwner;
};
