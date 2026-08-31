#pragma once

#include "CoreMinimal.h"
#include "UI/NKMUIActivatableWidget.h"

#include "CMStageResultWidget.generated.h"

class UButton;
class UTextBlock;

UCLASS(Blueprintable)
class UI_API UCMStageResultWidget : public UNKMUIActivatableWidget
{
    GENERATED_BODY()

public:
    UCMStageResultWidget(const FObjectInitializer& ObjectInitializer);

    void SetResult(
        float ElapsedSeconds,
        bool bCanControlResult,
        bool bHasNextStage);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;

private:
    UFUNCTION()
    void HandleRestartClicked();

    UFUNCTION()
    void HandleNextLevelClicked();

    UFUNCTION()
    void HandleMainMenuClicked();

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TimeText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> RestartButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> NextLevelButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> MainMenuButton;
};
