#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CMBodyTableRow.generated.h"

USTRUCT(BlueprintType)
struct CHIMERA_API FCMBodyTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName RowName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ID;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString BodyType;
};