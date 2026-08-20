#pragma once

#include "CoreMinimal.h"

#include "CMTestAreaTypes.generated.h"

USTRUCT(BlueprintType)
struct CHIMERA_API FCMTestAreaInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName AreaId;

    UPROPERTY(BlueprintReadOnly)
    FText DisplayName;

};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FCMTestAreaChangedSignature,
    FName, CurrentAreaId);
