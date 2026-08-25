#pragma once

#include "CoreMinimal.h"
#include "UI/NKMUIActivatableWidget.h"

#include "CMOptionWidget.generated.h"

class UButton;
class UComboBoxString;
class USlider;
class UTextBlock;

UCLASS(Abstract)
class UI_API UCMOptionWidget : public UNKMUIActivatableWidget
{
    GENERATED_BODY()

public:
    UCMOptionWidget(const FObjectInitializer& ObjectInitializer);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;
    virtual FReply NativeOnPreviewKeyDown(
        const FGeometry& InGeometry,
        const FKeyEvent& InKeyEvent) override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;

private:
    struct FResolutionPreset
    {
        FString Label;
        FIntPoint Size = FIntPoint::ZeroValue;
    };

    void RefreshFromRuntime();
    void RefreshVolumeText(UTextBlock* TextBlock, float Value) const;
    void ApplyVideoSettings(UGameUserSettings* UserSettings) const;

    UFUNCTION()
    void HandleCloseClicked();

    UFUNCTION()
    void HandleResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void HandleWindowModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void HandleLanguageChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void HandleMasterVolumeChanged(float Value);

    UFUNCTION()
    void HandleBGMVolumeChanged(float Value);

    UFUNCTION()
    void HandleSFXVolumeChanged(float Value);

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> Combo_Resolution;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> Combo_WindowMode;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> Combo_Language;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<USlider> Slider_MasterVolume;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<USlider> Slider_BGMVolume;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<USlider> Slider_SFXVolume;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_MasterVolume;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_BGMVolume;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_SFXVolume;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Btn_Close;

    TArray<FResolutionPreset> ResolutionPresets;
    bool bIsRefreshing = false;
};
