#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "EditorValidatorBase.h"
#include "RequirementCoverageValidator.generated.h"

// 필수 자산이 schedule에 등록됐는지 검사한다.
UCLASS()
class ASYNCPDALOADEREDITOR_API URequirementCoverageValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

public:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;
};

#endif // WITH_EDITOR

