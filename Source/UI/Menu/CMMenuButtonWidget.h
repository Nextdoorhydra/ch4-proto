#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "CMMenuButtonWidget.generated.h"

class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCMMenuButtonClicked);

UCLASS(Abstract, Blueprintable)
class UI_API UCMMenuButtonWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "Chimera|Menu Button")
    FCMMenuButtonClicked OnClicked;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    UFUNCTION()
    void HandleButtonClicked();

    UPROPERTY(Transient)
    TObjectPtr<UButton> InternalButton;
};
