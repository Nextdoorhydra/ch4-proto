#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "DataForgeAssetLayoutProfileFactory.generated.h"

UCLASS()
class DATAFORGEEDITOR_API UDataForgeAssetLayoutProfileFactory : public UFactory
{
	GENERATED_BODY()

public:
	UDataForgeAssetLayoutProfileFactory();

	virtual UObject* FactoryCreateNew(
		UClass* Class,
		UObject* InParent,
		FName Name,
		EObjectFlags Flags,
		UObject* Context,
		FFeedbackContext* Warn) override;
};
