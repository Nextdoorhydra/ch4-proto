#pragma once

#include "CoreMinimal.h"
#include "AsyncLoadCompleteMessage.h"
#include "Parts/Core/CMPartActorBase.h"

#include "CMHeadPartActor.generated.h"

class UCMVisionComponent;
class UCMHeadDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FCMHeadLookRotationChangedSignature,
    FRotator,
    LookRotation
);

/** A Head Part that contributes one cone to the team's shared vision. */
UCLASS(Blueprintable)
class CHIMERA_API ACMHeadPartActor : public ACMPartActorBase
{
    GENERATED_BODY()

public:
    ACMHeadPartActor();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    virtual void OnAttachedToPartSlot_Implementation(
        UCMPartSlotComponent* PartSlot
    ) override;

    virtual void OnDetachedFromPartSlot_Implementation(
        UCMPartSlotComponent* PartSlot
    ) override;

    UFUNCTION(BlueprintPure, Category = "Chimera|Part|Head")
    UCMVisionComponent* GetVisionComponent() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Part|Head")
    bool IsHeadDefinitionReady() const { return bDefinitionReady; }

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Part|Head|Animation")
    void SetProceduralLookRotation(FRotator InLookRotation);

    UFUNCTION(BlueprintPure, Category = "Chimera|Part|Head|Animation")
    FRotator GetProceduralLookRotation() const
    {
        return ProceduralLookRotation;
    }

    UPROPERTY(BlueprintAssignable,
        Category = "Chimera|Part|Head|Animation")
    FCMHeadLookRotationChangedSignature OnLookRotationChanged;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Chimera|Part|Head")
    TObjectPtr<UCMVisionComponent> VisionComponent;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part|Head|Definition")
    TSoftObjectPtr<UCMHeadDefinition> Definition;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera|Part|Head|Definition")
    FName LoadGroupId = TEXT("Stage.Entry.HeadVision");

private:
    UFUNCTION()
    void OnRep_ProceduralLookRotation();

    UFUNCTION()
    void HandleLoadGroupFinished(
        FName FinishedLoadGroupId,
        EAsyncLoadResult Result,
        bool bReleasedImmediately
    );

    void RefreshDefinitionState();
    bool TryResolveLoadedDefinition();
    void MarkDefinitionFailed(const TCHAR* Reason);
    void ApplyLoadedDefinition();

    bool bDefinitionReady = false;
    bool bDefinitionFailed = false;

    UPROPERTY(ReplicatedUsing = OnRep_ProceduralLookRotation)
    FRotator ProceduralLookRotation = FRotator::ZeroRotator;
};
