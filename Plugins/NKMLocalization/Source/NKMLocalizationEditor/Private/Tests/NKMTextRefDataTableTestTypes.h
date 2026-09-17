#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NKMTextRef.h"
#include "NKMTextRefDataTableTestTypes.generated.h"

USTRUCT()
struct FNKMTextRefDataTableTestRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY()
	FNKMTextRef DisplayName;
};
