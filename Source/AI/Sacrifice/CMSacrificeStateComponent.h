#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gore/CMDismemberableTarget.h"
#include "Gore/CMDismembermentDefinition.h"

#include "CMSacrificeStateComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCMSacrificePartSeveredSignature, ECMBodyPart, BodyPart);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCMSacrificeMissingPartsChangedSignature, int32, MissingPartCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCMSacrificeBleedingChangedSignature, bool, bBleeding);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCMSacrificeDiedSignature);

class UCMDismembermentComponent;
class ACMPartActorBase;
struct FCMSacrificeAttackPartRule;

/** Server-authoritative injury, bleeding, and death state for a Sacrifice. */
UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
class AI_API UCMSacrificeStateComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMSacrificeStateComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    int32 ResolveDismembermentHit(const FCMDismembermentHitRequest& Request);

    /** Centipede fatal attack: severs every attached part regardless of acquisition rules, then kills the victim. */
    int32 ResolveFatalDismembermentHit(const FCMDismembermentHitRequest& Request);

    UFUNCTION(BlueprintPure, Category = "Chimera|Sacrifice")
    bool IsAlive() const
    {
        return !bDead;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Sacrifice")
    bool HasBodyPart(ECMBodyPart BodyPart) const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Sacrifice")
    int32 GetMissingPartCount() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Sacrifice")
    int32 GetMissingPartMask() const
    {
        return MissingPartMask;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Sacrifice")
    TArray<ECMBodyPart> GetAttachedBodyParts() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Sacrifice")
    bool IsBleeding() const
    {
        return bBleeding;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Sacrifice")
    float GetBleedTimeRemaining() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Sacrifice|Rewards")
    bool HasRewardForBodyPart(ECMBodyPart BodyPart) const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Sacrifice|Rewards")
    bool HasRewardDropped(ECMBodyPart BodyPart) const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Sacrifice|Rewards")
    int32 GetRemainingRewardCount() const;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Sacrifice")
    FCMSacrificePartSeveredSignature OnBodyPartSevered;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Sacrifice")
    FCMSacrificeMissingPartsChangedSignature OnMissingPartsChanged;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Sacrifice")
    FCMSacrificeBleedingChangedSignature OnBleedingStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Sacrifice")
    FCMSacrificeDiedSignature OnSacrificeDied;

private:
    UFUNCTION()
    void OnRep_MissingPartMask(uint8 PreviousMask);

    UFUNCTION()
    void OnRep_Bleeding();

    UFUNCTION()
    void OnRep_Dead();

    void StartOrUpdateBleeding(int32 PreviousMissingCount, int32 NewlyMissingCount);
    void HandleBleedExpired();
    void Die();
    void ValidateAttackPartRules() const;
    const FCMSacrificeAttackPartRule* FindAttackPartRule(ECMBodyPart BodyPart) const;
    TSubclassOf<ACMPartActorBase> ResolveCollectiblePartClass(ECMBodyPart BodyPart) const;
    bool WasAttackAlreadyResolved(const FGuid& AttackId) const;
    void RememberResolvedAttack(const FGuid& AttackId);

    UPROPERTY(ReplicatedUsing = OnRep_MissingPartMask)
    uint8 MissingPartMask = 0;

    UPROPERTY(ReplicatedUsing = OnRep_Dead)
    bool bDead = false;

    UPROPERTY(ReplicatedUsing = OnRep_Bleeding)
    bool bBleeding = false;

    UPROPERTY(Replicated)
    float BleedEndTime = 0.0f;

    UPROPERTY(Replicated)
    uint8 DroppedRewardMask = 0;

    UPROPERTY(Transient)
    TObjectPtr<UCMDismembermentComponent> DismembermentComponent;

    TArray<FGuid> RecentAttackIds;
    FTimerHandle BleedTimerHandle;
};
