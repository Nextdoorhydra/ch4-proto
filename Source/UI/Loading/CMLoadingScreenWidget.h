#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "CMLoadingScreenWidget.generated.h"

class UTextBlock;
class UWidgetAnimation;

UCLASS(Blueprintable)
class UI_API UCMLoadingScreenWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetLoadingText(const FText& NewLoadingText);

protected:
    virtual void NativeConstruct() override;

private:
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> LoadingText;

    UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
    TObjectPtr<UWidgetAnimation> LoadingSpinnerAnimation;
};
