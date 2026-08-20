#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "CMTestAreaSelectorWidget.generated.h"

class UCMTestAreaRowWidget;
class UScrollBox;

UCLASS(Abstract)
// 로드된 Test Area 시작점을 자동으로 나열하는 선택 UI 베이스
class UI_API UCMTestAreaSelectorWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Chimera|Testing")
    void RefreshAreaList();

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Testing")
    TSubclassOf<UCMTestAreaRowWidget> AreaRowClass;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UScrollBox> SB_TestAreas;
};
