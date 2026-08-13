#pragma once

#include "CoreMinimal.h"
#include "DataForgeAssetLayoutProfile.h"
#include "DataForgeTypes.h"
#include "Engine/DataAsset.h"
#include "DataForgeRuleSet.generated.h"

class UDataForgeRuleSet;

USTRUCT(BlueprintType)
struct DATAFORGECORE_API FDataForgeDependencyRule
{
	GENERATED_BODY()

	/** RuleSet that must validate and execute before this RuleSet. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dependency")
	TSoftObjectPtr<UDataForgeRuleSet> RuleSet;
};

UCLASS(BlueprintType)
class DATAFORGECORE_API UDataForgeRuleSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DataForge")
	FGuid RuleSetId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DataForge", AdvancedDisplay, meta = (ClampMin = "1"))
	int32 RuleVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source")
	FDataForgeSourceConfig Source;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Schema")
	FDataForgeSchemaRule Schema;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output")
	FDataForgeDataTableOutputRule Output;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset Rules", AdvancedDisplay, meta = (ToolTip = "Optional asset lookup/creation rules. {ColumnName} tokens use canonical source column values."))
	TArray<FDataForgeAssetRule> AssetRules;

	/** Optional authoring provenance. AssetRules remain concrete and compile without the Profile. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Asset Layout", AdvancedDisplay)
	FDataForgeProfileOrigin ProfileOrigin;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generated Outputs", AdvancedDisplay)
	TArray<FDataForgeGeneratedAssetOutputRule> GeneratedOutputs;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bindings")
	TArray<FDataForgeBindingRule> Bindings;

	/** Explicit cross-RuleSet prerequisites. Array order does not affect execution order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dependencies", AdvancedDisplay)
	TArray<FDataForgeDependencyRule> Dependencies;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Transient, Category = "DataForge Status")
	FString LastStatus = TEXT("Draft");

	UPROPERTY(VisibleAnywhere, Transient, Category = "DataForge Status", meta = (MultiLine = true))
	FString LastSummary;

	UPROPERTY(VisibleAnywhere, Transient, Category = "DataForge Status")
	TArray<FName> LastDetectedColumns;

	UPROPERTY(VisibleAnywhere, Transient, Category = "DataForge Status")
	TArray<FDataForgeDiagnostic> LastDiagnostics;
#endif
};
