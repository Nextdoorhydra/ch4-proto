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
class UCMInteractionHighlightComponent;

/** Physical world pickup created by severing a harvestable body part. */
UCLASS(Blueprintable)
class CHIMERA_API ACMDroppedPartActor : public AActor
{
    GENERATED_BODY()

public:
    ACMDroppedPartActor();

    virtual void Tick(float DeltaSeconds) override;

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

    /** Server-authored world point shared by gameplay and client presentation. */
    UFUNCTION(BlueprintPure, Category = "Chimera|Part Drop")
    FVector GetAuthoritativePickupLocation() const;

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

protected:
    virtual void BeginPlay() override;

private:
    UFUNCTION()
    void OnRep_VisualDefinition();

    UFUNCTION()
    void OnRep_TentaclePulled();

    UFUNCTION()
    void OnRep_AuthoritativePickupLocation();

    void ApplyVisualDefinition();
    void ApplyPickupHighlightMaterial();
    void RefreshPickupHighlight();
    void UpdateAuthoritativePickupLocation();
    void ApplyAuthoritativePickupLocation();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
        meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USkeletalMeshComponent> PartMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components",
        meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCMInteractionHighlightComponent> InteractionHighlight;

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

    UPROPERTY(ReplicatedUsing = OnRep_AuthoritativePickupLocation)
    FVector_NetQuantize10 AuthoritativePickupLocation;

    bool bConsumed = false;
    TWeakObjectPtr<AActor> TentacleReservationOwner;

    friend class FCMTentacleBlueprintIntegrationTest;
};
