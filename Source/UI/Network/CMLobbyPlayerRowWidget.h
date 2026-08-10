#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "CMLobbyPlayerRowWidget.generated.h"

class UTextBlock;

UCLASS(Abstract, Blueprintable)
class UI_API UCMLobbyPlayerRowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetPlayerDisplayName(
        const FString& NewPlayerDisplayName,
        const FLinearColor& NewPlayerDisplayColor
    );

protected:
    virtual void NativePreConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_PlayerName;

private:
    UPROPERTY(Transient)
    FText PlayerDisplayName;

    FLinearColor PlayerDisplayColor = FLinearColor::White;
};
