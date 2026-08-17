#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/CMPartInterface.h"

#include "CMDebugPartActor.generated.h"

class UGameplayAbility;
class USceneComponent;
class ACMPlayerState;

/** Non-shipping Part used to verify attachment and slot-to-GA input routing. */
UCLASS(NotBlueprintable, NotPlaceable, Transient)
class CHIMERA_API ACMDebugPartActor
    : public AActor
    , public ICMPartInterface
{
    GENERATED_BODY()

public:
    ACMDebugPartActor();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    void InitializeDebugPart(ECMPartSlotType InPartType);

    virtual ECMPartSlotType GetPartType_Implementation() const override;
    virtual TSubclassOf<UGameplayAbility>
        GetGrantedAbilityClass_Implementation() const override;
    virtual void OnAttachedToPartSlot_Implementation(
        UCMPartSlotComponent* PartSlot) override;
    virtual void OnDetachedFromPartSlot_Implementation(
        UCMPartSlotComponent* PartSlot) override;

    const FCMPartSlotAddress& GetAttachedSlotAddress() const;
    FString GetPartTypeName() const;

    /** Supplies the player that caused the next diagnostic GA activation. */
    void SetContributingPlayerState(ACMPlayerState* PlayerState);
    ACMPlayerState* ConsumeContributingPlayerState();

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(Replicated)
    ECMPartSlotType PartType = ECMPartSlotType::Any;

    UPROPERTY(Replicated)
    FCMPartSlotAddress AttachedSlotAddress;

    TWeakObjectPtr<ACMPlayerState> PendingContributingPlayerState;
};
