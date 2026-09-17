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

    // 플레이어 이름과 Ready 여부를 한 행에 함께 표시
    void SetPlayerLobbyState(
        const FString& NewPlayerDisplayName,
        const FLinearColor& NewPlayerDisplayColor,
        bool bNewReady
    );

protected:
    virtual void NativePreConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_PlayerName;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_ReadyState;

private:
    UPROPERTY(Transient)
    FText PlayerDisplayName;

    FLinearColor PlayerDisplayColor = FLinearColor::White;

    bool bReady = false;
};
