#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Sound/NKMSoundTypes.h"
#include "NKMSoundCatalogProvider.generated.h"

UINTERFACE(MinimalAPI)
class UNKMSoundCatalogProvider : public UInterface
{
	GENERATED_BODY()
};

class NKMSOUNDRUNTIME_API INKMSoundCatalogProvider
{
	GENERATED_BODY()

public:
	virtual const TArray<FNKMSoundEntry>& GetSoundEntries() const = 0;
	virtual FPrimaryAssetId GetSoundCatalogId() const = 0;
};
