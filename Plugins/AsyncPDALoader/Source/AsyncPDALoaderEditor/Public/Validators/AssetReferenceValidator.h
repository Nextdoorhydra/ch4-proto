#pragma once

#include "CoreMinimal.h"

// Data Validation은 에디터에서만 사용한다.
#if WITH_EDITOR

#include "EditorValidatorBase.h"
#include "AssetReferenceValidator.generated.h"

// schedule의 자산 ID와 타입을 검사한다.
UCLASS()
class ASYNCPDALOADEREDITOR_API UAssetReferenceValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

public:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;
};

#endif // WITH_EDITOR

