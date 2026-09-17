#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "NKMUIExtensions.generated.h"

class APlayerController;
class ULocalPlayer;

UCLASS()
class NKMUI_API UNKMUIExtensions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "NKM UI Extensions")
	static ULocalPlayer* GetLocalPlayerFromController(APlayerController* PlayerController);
};
