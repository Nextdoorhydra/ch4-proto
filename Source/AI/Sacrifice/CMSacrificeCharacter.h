#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "Gore/CMDismemberableTarget.h"
#include "Gore/CMDismembermentDefinition.h"
#include "Sacrifice/CMSacrificeAITypes.h"

#include "CMSacrificeCharacter.generated.h"

class UCMDismembermentComponent;
class UCMSacrificeStateComponent;
class UCMSacrificeAttributeSet;
class UAudioComponent;
class UAbilitySystemComponent;
class UAnimMontage;
class UAnimSequence;
class UGameplayAbility;
class UMotionWarpingComponent;
class USkeletalMeshComponent;
class ACMPartActorBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCMSacrificeHitAcceptedSignature, AActor*, Attacker, AActor*, SourcePart, FVector, ImpactDirection);

/** Per-body-part rule that only controls whether the detached part is collectible. */
USTRUCT(BlueprintType)
struct AI_API FCMSacrificeAttackPartRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment")
    ECMBodyPart BodyPart = ECMBodyPart::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment")
    bool bPlayerCanAcquire = false;

    /** Optional override; when empty, the native Head/Arm/Leg part class is used. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment", meta = (EditCondition = "bPlayerCanAcquire", EditConditionHides))
    TSubclassOf<ACMPartActorBase> PartClassOverride;
};

/** Character foundation for the Sacrifice AI; contains no AI behavior. */
UCLASS(Blueprintable)
class AI_API ACMSacrificeCharacter : public ACharacter, public IAbilitySystemInterface, public ICMDismemberableTarget
{
    GENERATED_BODY()

public:
    ACMSacrificeCharacter();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    virtual int32 ReceiveDismembermentHit_Implementation(const FCMDismembermentHitRequest& Request) override;

    UFUNCTION(BlueprintPure, Category = "Chimera|Sacrifice")
    UCMSacrificeStateComponent* GetSacrificeStateComponent() const
    {
        return SacrificeStateComponent;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Sacrifice")
    UCMDismembermentComponent* GetDismembermentComponent() const
    {
        return DismembermentComponent;
    }

    const TArray<FCMSacrificeAttackPartRule>& GetAttackPartRules() const
    {
        return AttackPartRules;
    }

    float GetDismembermentImpulse() const
    {
        return DismembermentImpulse;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Sacrifice|AI")
    ECMSacrificeActionState GetSacrificeActionState() const
    {
        return CurrentActionState;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Sacrifice|AI")
    AActor* GetCurrentThreat() const
    {
        return CurrentThreat;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Sacrifice|Animation")
    bool IsHitReacting() const
    {
        return bHitReacting;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Sacrifice|Animation")
    ECMSacrificeHitReactionDirection GetHitReactionDirection() const
    {
        return HitReactionDirection;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Sacrifice|Animation")
    ECMSacrificeHitReactionDirection GetGettingUpDirection() const
    {
        return GettingUpDirection;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Sacrifice|Animation")
    bool ShouldUseFrontGettingUpAnimation() const
    {
        return GettingUpDirection == ECMSacrificeHitReactionDirection::Front;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Sacrifice|Animation")
    FVector GetHitKnockbackDirection() const
    {
        return HitKnockbackDirection;
    }

    UFUNCTION(BlueprintPure, Category = "Chimera|Sacrifice|AI")
    bool HasLostLeg() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Sacrifice|AI")
    bool HasLostArmOrLeg() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Sacrifice|GAS")
    float GetFleeCharges() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Sacrifice|GAS")
    float GetMaxFleeCharges() const;
    bool HasLostArm() const;

    FVector GetSacrificeVisionOrigin() const;
    FVector GetSacrificeVisionForward() const;

    bool ActivateSacrificeAction(TSubclassOf<UGameplayAbility> AbilityClass);
    void CancelSacrificeActions();
    void SetSacrificeActionState(ECMSacrificeActionState NewState, FGameplayTag StateTag);
    void ClearSacrificeActionState(ECMSacrificeActionState State, FGameplayTag StateTag);
    void SetCurrentThreat(AActor* NewThreat);
    void PlayThreatScream();
    void ConsumeFleeCharge();
    void RefillFleeCharges();
    float BeginHitReaction(const FVector& ImpactDirection);
    float BeginSafetyInjuryFall();
    void FinishHitReaction();
    float BeginBackFallRootMotion();
    void FinishBackFallRootMotion();
    float GetBackFallDuration() const;
    float BeginGettingUp(ECMSacrificeHitReactionDirection Direction);
    void FinishGettingUp();
    float GetGettingUpDuration(ECMSacrificeHitReactionDirection Direction) const;
    void EnterIncapacitated();
    bool EnterPendingIncapacitation();

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Sacrifice|AI")
    FCMSacrificeHitAcceptedSignature OnSacrificeHitAccepted;

protected:
    UFUNCTION()
    void HandleBodyPartSevered(ECMBodyPart BodyPart);

    UFUNCTION()
    void HandleMissingPartsChanged(int32 MissingPartCount);

    UFUNCTION()
    void HandleBleedingStateChanged(bool bBleeding);

    UFUNCTION()
    void HandleSacrificeDied();

    void InitializeAbilitySystem();
    void GrantActionAbilities();
    void ApplyStateTag(FGameplayTag StateTag, bool bEnabled);
    float CalculateMovementSpeed() const;
    void RefreshMovementSpeed();
    void RefreshBleedingLoopSound(bool bBleeding);
    void StopBleedingLoopSound();

    /** Optional per-part collectible settings; these rules never restrict which attached part can be severed. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Sacrifice|Dismemberment", meta = (TitleProperty = "BodyPart", DisplayName = "Attack Part Rules"))
    TArray<FCMSacrificeAttackPartRule> AttackPartRules;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Sacrifice|Dismemberment", meta = (ClampMin = "0.0"))
    float DismembermentImpulse = 1200.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UCMDismembermentComponent> DismembermentComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UCMSacrificeStateComponent> SacrificeStateComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UCMSacrificeAttributeSet> SacrificeAttributeSet;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body Parts")
    TObjectPtr<USkeletalMeshComponent> Head;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body Parts")
    TObjectPtr<USkeletalMeshComponent> Arn_L;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body Parts")
    TObjectPtr<USkeletalMeshComponent> Arn_R;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body Parts")
    TObjectPtr<USkeletalMeshComponent> Leg_L;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body Parts")
    TObjectPtr<USkeletalMeshComponent> Leg_R;

    /** Bone or socket that supplies the animated vision transform. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Sacrifice|AI")
    FName VisionBoneName = TEXT("head");

    /** Anatomical forward direction in VisionBoneName's local space. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Sacrifice|AI")
    FVector VisionBoneLocalForwardAxis = FVector::YAxisVector;

    /** Converts the imported +Y-facing mesh to Unreal Character +X forward. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Sacrifice|Movement", meta = (ClampMin = "-180.0", ClampMax = "180.0", ForceUnits = "deg"))
    float CharacterMeshYawOffsetDegrees = -90.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Sacrifice|Movement", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
    float WanderWalkSpeed = 100.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Sacrifice|Movement", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
    float DefaultWalkSpeed = 300.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Sacrifice|Movement", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
    float FleeRunSpeed = 400.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Sacrifice|Movement", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
    float BackCrawlSpeed = 120.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Sacrifice|Movement", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
    float InjuredCrawlSpeed = 100.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Sacrifice|Movement", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
    float ExhaustedWalkSpeed = 150.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Sacrifice|Movement", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
    float InjuredArmMaxSpeed = 200.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Sacrifice|Movement", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
    float InjuredLegMaxSpeed = 100.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Sacrifice|Animation")
    TObjectPtr<UAnimSequence> FrontHitFallAnimation;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Sacrifice|Animation")
    TObjectPtr<UAnimSequence> BackHitFallAnimation;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Sacrifice|Animation")
    TObjectPtr<UAnimSequence> BackFallAnimation;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Sacrifice|Animation")
    TObjectPtr<UAnimSequence> SafetyInjuryFallAnimation;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Sacrifice|Animation")
    TObjectPtr<UAnimSequence> StandingUpBackAnimation;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Sacrifice|Animation")
    TObjectPtr<UAnimSequence> StandingUpFrontAnimation;

    /** Keeps the get-up montage over the state-machine transition to movement. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Sacrifice|Animation", meta = (ClampMin = "0.0", ForceUnits = "s"))
    float GettingUpStateTransitionLeadTime = 0.25f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Sacrifice|Knockback", meta = (ClampMin = "0.0", ForceUnits = "cm"))
    float HitKnockbackDistanceCm = 150.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Sacrifice|Movement", meta = (ClampMin = "0.0", ForceUnits = "cm"))
    float StumbleBackwardDistanceCm = 180.0f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Chimera|Sacrifice|AI")
    ECMSacrificeActionState CurrentActionState = ECMSacrificeActionState::None;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Chimera|Sacrifice|AI")
    TObjectPtr<AActor> CurrentThreat;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Chimera|Sacrifice|Animation")
    bool bHitReacting = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Chimera|Sacrifice|Animation")
    ECMSacrificeHitReactionDirection HitReactionDirection = ECMSacrificeHitReactionDirection::None;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Chimera|Sacrifice|Animation")
    ECMSacrificeHitReactionDirection GettingUpDirection = ECMSacrificeHitReactionDirection::Back;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Chimera|Sacrifice|Knockback")
    FVector HitKnockbackDirection = FVector::ZeroVector;

private:
    void ConfigureWarpedAnimation(UAnimSequence* Animation, FName WarpTargetName, const FTransform& TargetTransform);
    void ClearMotionWarping();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastConfigureHitWarp(ECMSacrificeHitReactionDirection Direction, FVector TargetLocation, FRotator TargetRotation);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastConfigureBackFallWarp(FVector TargetLocation, FRotator TargetRotation);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastClearMotionWarping();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastPlayGettingUpAnimation(ECMSacrificeHitReactionDirection Direction);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastStopGettingUpAnimation();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastPlaySafetyInjuryFallAnimation();

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastPlayVocalSound(FGameplayTag SoundTag);

    UPROPERTY(Transient)
    TObjectPtr<UAnimMontage> ActiveGettingUpMontage;

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> BleedingLoopSoundComponent;

    bool bPendingIncapacitation = false;
    TMap<FGameplayTag, FActiveGameplayEffectHandle> StateEffectHandles;
};
