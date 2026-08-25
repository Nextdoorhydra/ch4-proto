#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "CMBloodDefinitionRegistry.generated.h"

class UCMBloodDefinition;


/**
 * CMGore가 사용할 Blood Definition들의 명시적인 Registry.
 * Definition을 hard reference로 두기 위함
 */
UCLASS(BlueprintType)
class CMGORE_API UCMBloodDefinitionRegistry : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Definitions")
	TArray<TObjectPtr<UCMBloodDefinition>> Definitions;
};