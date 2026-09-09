#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "Ping/CMPingTypes.h"

#include "CMWorldPingWidget.generated.h"

class SImage;
class STextBlock;
class UTexture2D;

/** Native world-space marker used by replicated ping actors. */
UCLASS()
class CHIMERA_API UCMWorldPingWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetPresentation(
        ECMPingType Type,
        const FString& PlayerName,
        const FLinearColor& PlayerColor);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    void ApplyPresentation();
    UTexture2D* LoadIcon() const;

    ECMPingType PingType = ECMPingType::GoHere;
    FString PingingPlayerName;
    FLinearColor PingingPlayerColor = FLinearColor::White;
    FSlateBrush IconBrush;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> IconTexture;

    TSharedPtr<SImage> IconImage;
    TSharedPtr<STextBlock> PlayerNameText;
};
