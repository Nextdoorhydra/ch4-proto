#include "DataForgeAssetLayoutProfileFactory.h"

#include "DataForgeAssetLayoutProfile.h"

UDataForgeAssetLayoutProfileFactory::UDataForgeAssetLayoutProfileFactory()
{
	SupportedClass = UDataForgeAssetLayoutProfile::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UDataForgeAssetLayoutProfileFactory::FactoryCreateNew(
	UClass* Class,
	UObject* InParent,
	FName Name,
	EObjectFlags Flags,
	UObject* Context,
	FFeedbackContext* Warn)
{
	UDataForgeAssetLayoutProfile* Profile = NewObject<UDataForgeAssetLayoutProfile>(InParent, Class, Name, Flags | RF_Transactional);
	Profile->ProfileId = FGuid::NewGuid();
	return Profile;
}
