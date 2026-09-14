#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"

#include "CMPartVendingMachineWidget.generated.h"

class SImage;
class STextBlock;
class UTexture2D;

/** 자판기 종류 아이콘과 남은 횟수를 표시하는 월드 UI다. */
UCLASS()
class CHIMERA_API UCMPartVendingMachineWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetPresentation(UTexture2D* InPartIcon, int32 InRemainingUses);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    void ApplyPresentation();

    FSlateBrush IconBrush;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> PartIcon;

    int32 RemainingUses = 99;
    TSharedPtr<SImage> IconImage;
    TSharedPtr<STextBlock> RemainingUsesText;
};
