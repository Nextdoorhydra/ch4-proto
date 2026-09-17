#pragma once

#include "CommonActionWidget.h"
#include "NKMUIActionWidget.generated.h"

class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;

UCLASS(BlueprintType, Blueprintable)
class NKMUI_API UNKMUIActionWidget : public UCommonActionWidget
{
	GENERATED_BODY()

public:
	virtual FSlateBrush GetIcon() const override;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> AssociatedInputAction;

private:
	UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem() const;
};
