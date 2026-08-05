#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ChimeraLobbyPlayerRowWidget.generated.h"

class UTextBlock;

UCLASS(Abstract, Blueprintable)
class CHIMERA_API UChimeraLobbyPlayerRowWidget : public UUserWidget
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
