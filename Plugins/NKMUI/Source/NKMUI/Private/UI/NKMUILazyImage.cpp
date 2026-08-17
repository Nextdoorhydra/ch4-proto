#include "UI/NKMUILazyImage.h"

void UNKMUILazyImage::SetLazyTexture(TSoftObjectPtr<UTexture2D> Texture)
{
	SetBrushFromLazyTexture(Texture, bMatchTextureSize);
}

void UNKMUILazyImage::SetLazyDisplayAsset(TSoftObjectPtr<UObject> Asset)
{
	SetBrushFromLazyDisplayAsset(Asset, bMatchTextureSize);
}

void UNKMUILazyImage::ClearLazyImage()
{
	SetBrushFromLazyDisplayAsset(TSoftObjectPtr<UObject>(), bMatchTextureSize);
}
