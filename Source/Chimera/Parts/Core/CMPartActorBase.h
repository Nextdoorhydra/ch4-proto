#pragma once

#include "CoreMinimal.h"
#include "Data/Part/CMPartTier.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Player/CMPartInterface.h"

#include "CMPartActorBase.generated.h"

class UGameplayAbility;
class UCMBattleComponent;
class UCMPartStatusComponent;
class UBoxComponent;
class USceneComponent;
class USkeletalMeshComponent;
class UDataTable;
class ACMPlayerState;
struct FCMPartLegArmTableRow;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FCMPartHealthChangedSignature,
    float,
    PreviousHealth,
    float,
    CurrentHealth,
    float,
    MaxHealth
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCMPartDiedSignature);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FCMPartDisabledChangedSignature,
    bool,
    bDisabled
);

/**
 * Common base for replicated Parts that can be attached to a Chimera slot.
 *
 * Concrete Head, Arm, Leg, and Organ actors only need to configure PartType
 * and GrantedAbilityClass, then add their own behavior in derived classes.
 */
UCLASS(Abstract, Blueprintable)
class CHIMERA_API ACMPartActorBase
    : public AActor
    , public ICMPartInterface
{
    GENERATED_BODY()

public:
    ACMPartActorBase();

    /** Spawns one shared Part class and applies both data rows before BeginPlay. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Part",
        meta = (WorldContext = "WorldContextObject"))
    static ACMPartActorBase* SpawnPartFromDataRows(
        UObject* WorldContextObject,
        TSubclassOf<ACMPartActorBase> PartClass,
        FName InPartRowName,
        FName InTierRowName,
        const FTransform& SpawnTransform,
        AActor* InOwner
    );

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    virtual ECMPartSlotType GetPartType_Implementation() const override;

    virtual TSubclassOf<UGameplayAbility>
        GetGrantedAbilityClass_Implementation() const override;

    virtual void OnAttachedToPartSlot_Implementation(
        UCMPartSlotComponent* PartSlot
    ) override;

    virtual void OnDetachedFromPartSlot_Implementation(
        UCMPartSlotComponent* PartSlot
    ) override;

    UFUNCTION(BlueprintPure, Category = "Chimera|Part")
    UCMPartSlotComponent* GetAttachedPartSlot() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Part")
    FCMPartSlotAddress GetAttachedSlotAddress() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Part")
    UCMBattleComponent* GetBattleComponent() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Part")
    UCMPartStatusComponent* GetPartStatusComponent() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Part")
    USkeletalMeshComponent* GetPartMesh() const { return PartMesh; }

    /** Simple query-only collision used to identify this Part without bone lookup. */
    UFUNCTION(BlueprintPure, Category = "Chimera|Part")
    UBoxComponent* GetDamageHurtbox() const { return DamageHurtbox; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Part")
    float GetHealth() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Part")
    float GetMaxHealth() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Part")
    float GetStrength() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Part")
    float GetMovementImpulseMultiplier() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Part")
    float GetMovementImpulse() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Part")
    ECMPartTier GetTier() const { return Tier; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Part")
    int32 GetTierLevel() const { return CMPartTier::ToLevel(Tier); }

    UFUNCTION(BlueprintPure, Category = "Chimera|Part")
    FName GetTierRowName() const { return TierRowName; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Part")
    bool IsAlive() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Part")
    bool IsDisabled() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Part")
    bool IsAttached() const;

    /** True only while this Part can respond to its assigned control input. */
    UFUNCTION(BlueprintPure, Category = "Chimera|Part")
    bool IsOperational() const;

    /** Server-owned HP change used after BattleComponent resolves a hit. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Part")
    bool ApplyPartDamage(float Damage);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Part")
    void SetPartDisabled(bool bNewDisabled);

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Part")
    FCMPartHealthChangedSignature OnHealthChanged;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Part")
    FCMPartDiedSignature OnPartDied;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Part")
    FCMPartDisabledChangedSignature OnDisabledChanged;

    /** One-shot server context consumed by the Part GA activated from a shared ASC. */
    void SetContributingPlayerState(ACMPlayerState* PlayerState);
    ACMPlayerState* ConsumeContributingPlayerState();

    /** Server-only reservation while a segment tentacle pulls this loose Part. */
    bool TryReserveForTentacle(AActor* Requester);
    void ReleaseTentacleReservation(AActor* Requester);
    bool IsReservedForTentacle(const AActor* Requester = nullptr) const;
    bool IsReservedByTentacle(const AActor* Requester) const;

protected:
    virtual void BeginPlay() override;

    /** Lets Arm and Leg consume their type-specific columns after common data. */
    virtual void ApplyPartData(const FCMPartLegArmTableRow& PartRow);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USkeletalMeshComponent> PartMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> DamageHurtbox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UCMBattleComponent> BattleComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UCMPartStatusComponent> PartStatusComponent;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Part")
    ECMPartSlotType PartType = ECMPartSlotType::Any;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Part")
    TSubclassOf<UGameplayAbility> GrantedAbilityClass;

    UPROPERTY(EditDefaultsOnly, ReplicatedUsing = OnRep_MaxHealth,
        BlueprintReadOnly, Category = "Chimera|Part",
        meta = (ClampMin = "1.0"))
    float MaxHealth = 100.0f;

    UPROPERTY(EditDefaultsOnly, Replicated, BlueprintReadOnly,
        Category = "Chimera|Part", meta = (ClampMin = "0.0"))
    float Strength = 10.0f;

    UPROPERTY(VisibleInstanceOnly, Replicated, BlueprintReadOnly,
        Category = "Chimera|Part", meta = (ClampMin = "0.0"))
    float MovementImpulseMultiplier = 0.0f;

    UPROPERTY(VisibleInstanceOnly, Replicated, BlueprintReadOnly,
        Category = "Chimera|Part", meta = (ClampMin = "0.0"))
    float BaseMovementImpulse = 0.0f;

    // Google Sheet Loader/DataForge가 갱신하는 공용 Arm/Leg DataTable이다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part Data")
    TSoftObjectPtr<UDataTable> PartDataTable;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part Data")
    TSoftObjectPtr<UDataTable> PartTierDataTable;

    UPROPERTY(EditDefaultsOnly, Replicated, BlueprintReadOnly,
        Category = "Chimera|Part Data")
    FName PartRowName = NAME_None;

    UPROPERTY(EditDefaultsOnly, Replicated, BlueprintReadOnly,
        Category = "Chimera|Part Data")
    FName TierRowName = TEXT("Tier1");

    UPROPERTY(VisibleInstanceOnly, Replicated, BlueprintReadOnly,
        Category = "Chimera|Part Data")
    ECMPartTier Tier = ECMPartTier::Tier1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Chimera|Part Data")
    FName PartDataID = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Chimera|Part Data")
    FName Species = NAME_None;

    UPROPERTY(EditDefaultsOnly, Replicated, BlueprintReadOnly,
        Category = "Chimera|Part")
    FGameplayTagContainer PartStateTags;

private:
    bool InitializeFromPartData();

    UFUNCTION()
    void OnRep_MaxHealth();

    UFUNCTION()
    void OnRep_Health(float PreviousHealth);

    UFUNCTION()
    void OnRep_Dead();

    UFUNCTION()
    void OnRep_Disabled();

    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Part",
        meta = (AllowPrivateAccess = "true"))
    FCMPartSlotAddress AttachedSlotAddress;

    UPROPERTY(ReplicatedUsing = OnRep_Health,
        VisibleInstanceOnly, BlueprintReadOnly, Category = "Chimera|Part",
        meta = (AllowPrivateAccess = "true"))
    float Health = 0.0f;

    UPROPERTY(ReplicatedUsing = OnRep_Dead,
        VisibleInstanceOnly, BlueprintReadOnly, Category = "Chimera|Part",
        meta = (AllowPrivateAccess = "true"))
    bool bDead = false;

    UPROPERTY(ReplicatedUsing = OnRep_Disabled,
        VisibleInstanceOnly, BlueprintReadOnly, Category = "Chimera|Part",
        meta = (AllowPrivateAccess = "true"))
    bool bDisabled = false;

    TWeakObjectPtr<UCMPartSlotComponent> AttachedPartSlot;
    TWeakObjectPtr<ACMPlayerState> PendingContributingPlayerState;
    TWeakObjectPtr<AActor> TentacleReservationOwner;
};
