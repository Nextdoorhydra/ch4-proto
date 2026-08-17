#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "EditorValidatorBase.h"
#include "ScheduleReachabilityValidator.generated.h"

// 실제 요청되지 않는 schedule 항목을 검사한다.
UCLASS()
class ASYNCPDALOADEREDITOR_API UScheduleReachabilityValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

public:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;
};

#endif // WITH_EDITOR

