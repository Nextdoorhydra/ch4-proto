#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Gore/CMDismemberableTarget.h"
#include "Gore/CMDismembermentDefinition.h"

#include "CMSacrificeCharacter.generated.h"

class UCMDismembermentComponent;
class UCMSacrificeStateComponent;
class USkeletalMeshComponent;
class ACMPartActorBase;

/** One guaranteed collectible configured on a placed Sacrifice instance. */
USTRUCT(BlueprintType)
struct AI_API FCMSacrificeRewardPart
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Sacrifice|Reward")
    ECMBodyPart BodyPart = ECMBodyPart::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Sacrifice|Reward")
    TSubclassOf<ACMPartActorBase> PartClass;
};

/** Character foundation for the Sacrifice AI; contains no AI behavior. */
UCLASS(Blueprintable)
class AI_API ACMSacrificeCharacter
    : public ACharacter
    , public ICMDismemberableTarget
{
    GENERATED_BODY()

public:
    ACMSacrificeCharacter();

    virtual int32 ReceiveDismembermentHit_Implementation(
        const FCMDismembermentHitRequest& Request
    ) override;

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

    const TArray<FCMSacrificeRewardPart>& GetRewardParts() const
    {
        return RewardParts;
    }

protected:
    /** Rewards authored per placed victim; duplicate body parts are ignored. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "Chimera|Sacrifice|Rewards",
        meta = (TitleProperty = "BodyPart"))
    TArray<FCMSacrificeRewardPart> RewardParts;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UCMDismembermentComponent> DismembermentComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UCMSacrificeStateComponent> SacrificeStateComponent;

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
};
