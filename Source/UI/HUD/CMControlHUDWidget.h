#pragma once

#include "CoreMinimal.h"
#include "UI/NKMUIActivatableWidget.h"

#include "CMControlHUDWidget.generated.h"

class ACMControlBody;
class ACMChimera;
class ACMPartActorBase;
class ACMPlayerState;
class AActor;
class UBorder;
class UImage;
class UCMPartSlotComponent;
class UProgressBar;
class UTextBlock;
class UTexture2D;
class UWidget;
struct FOnAttributeChangeData;

/** WBP-authored Chimera body map; C++ only binds live gameplay data. */
UCLASS(Blueprintable)
class UI_API UCMControlHUDWidget
    : public UNKMUIActivatableWidget
{
    GENERATED_BODY()

public:
    UCMControlHUDWidget(
        const FObjectInitializer& ObjectInitializer);

    void SetControlBody(ACMControlBody* NewControlBody);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera HUD|Part Images")
    TObjectPtr<UTexture2D> HeadPartTexture;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera HUD|Part Images")
    TObjectPtr<UTexture2D> ArmPartTexture;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Chimera HUD|Part Images")
    TObjectPtr<UTexture2D> LegPartTexture;

private:
    struct FPartSlotVisual
    {
        TObjectPtr<UWidget> Root;
        TObjectPtr<UImage> BaseImage;
        TObjectPtr<UProgressBar> HealthFill;
        TObjectPtr<UTextBlock> KeyText;
        TObjectPtr<UTextBlock> PartText;
    };

    struct FSegmentRowVisual
    {
        TObjectPtr<UWidget> Root;
        TObjectPtr<UBorder> BodyBorder;
        TObjectPtr<UProgressBar> BodyHealthFill;
        TObjectPtr<UTextBlock> BodyControlText;
        TObjectPtr<UBorder> BodyStrikeLine;
    };

    bool CacheWidgetTreeReferences();
    void BindStateDelegates();
    void UnbindStateDelegates();
    void RebindObservedPlayerState();
    void RebindObservedSlotsAndParts();
    void UnbindObservedSlotsAndParts();
    void RefreshAll();
    void RefreshStamina();
    void RefreshBodySegments();
    void RefreshAssignedParts();
    void RefreshPartHealth();
    void ResetPartSlotVisual(FPartSlotVisual& Visual);
    void SetControlSlotHighlighted(int32 ControlIndex, bool bPressed);
    UTexture2D* GetPartTexture(const ACMPartActorBase* PartActor) const;
    FText GetPartLabel(const ACMPartActorBase* PartActor) const;

    UFUNCTION()
    void HandleControlSlotsChanged();

    UFUNCTION()
    void HandleControlInputChanged(int32 SlotIndex, bool bPressed);

    UFUNCTION()
    void HandleControlPlayerStateChanged();

    UFUNCTION()
    void HandleSegmentStatesChanged();

    UFUNCTION()
    void HandlePartAttachmentChanged(
        UCMPartSlotComponent* PartSlot,
        AActor* AttachedPart
    );

    UFUNCTION()
    void HandlePartHealthChanged(
        float PreviousHealth,
        float CurrentHealth,
        float MaxHealth
    );

    UFUNCTION()
    void HandlePlayerColorChanged();

    void HandleStaminaChanged(const FOnAttributeChangeData& ChangeData);

    TWeakObjectPtr<ACMControlBody> ControlBody;
    TWeakObjectPtr<ACMChimera> SharedChimera;
    TWeakObjectPtr<ACMPlayerState> ObservedPlayerState;

    UPROPERTY(Transient)
    TObjectPtr<UWidget> CachedHUDContainer;

    UPROPERTY(Transient)
    TObjectPtr<UProgressBar> CachedStaminaProgressBar;

    TArray<FSegmentRowVisual> SegmentRows;
    TArray<FPartSlotVisual> PhysicalPartSlots;
    TArray<TWeakObjectPtr<UCMPartSlotComponent>> ObservedPartSlots;
    TArray<TWeakObjectPtr<ACMPartActorBase>> ObservedParts;
    FDelegateHandle StaminaChangedHandle;
    FDelegateHandle MaxStaminaChangedHandle;
};
