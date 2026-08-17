#pragma once

#include "CommonUserWidget.h"
#include "CoreMinimal.h"
#include "NKMUIUserWidget.generated.h"

UCLASS(Abstract, Blueprintable)
class NKMUI_API UNKMUIUserWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UNKMUIUserWidget(const FObjectInitializer& ObjectInitializer);
};
