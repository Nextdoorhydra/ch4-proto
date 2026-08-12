#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "DataForgeCommandlet.generated.h"

UCLASS()
class DATAFORGEEDITOR_API UDataForgeCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UDataForgeCommandlet();
	virtual int32 Main(const FString& Params) override;
};
