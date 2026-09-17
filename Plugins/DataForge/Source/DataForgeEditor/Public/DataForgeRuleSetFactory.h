#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "DataForgeRuleSetFactory.generated.h"

UCLASS()
class DATAFORGEEDITOR_API UDataForgeRuleSetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UDataForgeRuleSetFactory();

	virtual UObject* FactoryCreateNew(
		UClass* Class,
		UObject* InParent,
		FName Name,
		EObjectFlags Flags,
		UObject* Context,
		FFeedbackContext* Warn) override;
};
