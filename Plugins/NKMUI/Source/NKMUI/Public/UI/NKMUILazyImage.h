#pragma once

#include "CommonLazyImage.h"
#include "CoreMinimal.h"
#include "NKMUILazyImage.generated.h"

/**
 * MVVM-friendly CommonUI image for on-demand soft Texture/Material loading.
 * Bind a ViewModel soft pointer directly to one of the setter functions instead
 * of converting an unloaded object to a SlateBrush.
 */
UCLASS()
class NKMUI_API UNKMUILazyImage : public UCommonLazyImage
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "NKM|UI|Assets")
	void SetLazyTexture(TSoftObjectPtr<UTexture2D> Texture);

	UFUNCTION(BlueprintCallable, Category = "NKM|UI|Assets")
	void SetLazyDisplayAsset(TSoftObjectPtr<UObject> Asset);

	UFUNCTION(BlueprintCallable, Category = "NKM|UI|Assets")
	void ClearLazyImage();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NKM|UI|Assets")
	bool bMatchTextureSize = false;
};
