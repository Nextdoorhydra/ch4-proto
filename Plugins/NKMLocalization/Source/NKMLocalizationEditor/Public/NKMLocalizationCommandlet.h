#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "NKMLocalizationCommandlet.generated.h"

UCLASS()
class NKMLOCALIZATIONEDITOR_API UNKMLocalizationCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UNKMLocalizationCommandlet();

	virtual int32 Main(const FString& Params) override;
};
