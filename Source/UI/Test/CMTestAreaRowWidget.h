#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "CMTestAreaRowWidget.generated.h"

class UButton;
class UTextBlock;

UCLASS(Abstract)
// Test Area 한 행의 이름 표시와 호스트 텔레포트 요청 담당
class UI_API UCMTestAreaRowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void InitializeArea(FName NewAreaId, const FText& NewDisplayName, bool bSelectionEnabled);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Btn_Select;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Txt_AreaName;

private:
    UFUNCTION()
    void HandleSelectClicked();

    FName AreaId;
};
