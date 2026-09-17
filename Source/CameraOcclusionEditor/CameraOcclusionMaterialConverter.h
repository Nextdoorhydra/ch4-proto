#pragma once

#include "CoreMinimal.h"

class CAMERAOCCLUSIONEDITOR_API FCameraOcclusionMaterialConverter
{
public:
    static void ConvertSelectedMaterials(const TArray<struct FAssetData>& SelectedAssets);
    static void ConvertMaterialsUnderFolders(const TArray<FString>& SelectedPackagePaths);
};
