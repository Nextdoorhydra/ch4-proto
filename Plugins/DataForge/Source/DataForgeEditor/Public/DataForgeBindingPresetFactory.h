#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "DataForgeBindingPresetFactory.generated.h"

UCLASS()
class DATAFORGEEDITOR_API UDataForgeBindingPresetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UDataForgeBindingPresetFactory();

	virtual UObject* FactoryCreateNew(
		UClass* Class,
		UObject* InParent,
		FName Name,
		EObjectFlags Flags,
		UObject* Context,
		FFeedbackContext* Warn) override;
};
