#include "DataForgeBindingPresetFactory.h"

#include "DataForgeBindingPreset.h"

UDataForgeBindingPresetFactory::UDataForgeBindingPresetFactory()
{
	SupportedClass = UDataForgeBindingPreset::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UDataForgeBindingPresetFactory::FactoryCreateNew(
	UClass* Class,
	UObject* InParent,
	FName Name,
	EObjectFlags Flags,
	UObject* Context,
	FFeedbackContext* Warn)
{
	UDataForgeBindingPreset* Preset = NewObject<UDataForgeBindingPreset>(InParent, Class, Name, Flags | RF_Transactional);
	Preset->PresetId = FGuid::NewGuid();
	return Preset;
}
