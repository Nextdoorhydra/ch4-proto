#include "DataForgeBindingPreset.h"

void UDataForgeBindingPreset::PostInitProperties()
{
	Super::PostInitProperties();
	if (!HasAnyFlags(RF_ClassDefaultObject) && !PresetId.IsValid())
	{
		PresetId = FGuid::NewGuid();
	}
}
