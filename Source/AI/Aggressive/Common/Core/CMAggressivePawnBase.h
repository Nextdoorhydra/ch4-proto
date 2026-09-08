#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "ActiveGameplayEffectHandle.h"
#include "Combat/CMCombatHitTarget.h"
#include "Common/Core/CMThreatSource.h"
#include "GameFramework/Pawn.h"

#include "CMAggressivePawnBase.generated.h"

class UAbilitySystemComponent;
class UCMAggressiveAIAttributeSet;
class UCMAggressiveKnockbackComponent;

/** Shared GAS/threat/hit foundation; concrete pawns retain their movement code. */
UCLASS(Abstract)
class AI_API ACMAggressivePawnBase : public APawn, public IAbilitySystemInterface, public ICMThreatSource, public ICMCombatHitTarget
{
    GENERATED_BODY()

public:
    ACMAggressivePawnBase();
    virtual void BeginPlay() override;
    virtual void SpawnDefaultController() override;
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual bool ReceiveCombatHit_Implementation(const FCMCombatHitRequest& Request) override;

    UCMAggressiveKnockbackComponent* GetKnockbackComponent() const
    {
        return KnockbackComponent;
    }
    bool StartConfiguredKnockback();
    void SetKnockbackState(bool bEnabled);

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aggressive AI|GAS", meta = (ClampMin = "0.0"))
    float ConfiguredKnockbackDistanceCm = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UCMAggressiveAIAttributeSet> AggressiveAttributeSet;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UCMAggressiveKnockbackComponent> KnockbackComponent;

private:
    bool WasAttackAlreadyResolved(const FGuid& AttackId) const;
    void RememberResolvedAttack(const FGuid& AttackId);

    FVector PendingKnockbackDirection = FVector::ForwardVector;
    TArray<FGuid> RecentAttackIds;
    FActiveGameplayEffectHandle KnockbackStateHandle;
};
