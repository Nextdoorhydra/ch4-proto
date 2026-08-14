#pragma once

#include "CoreMinimal.h"
#include "DataForgeAuthoringPlanner.h"
#include "UObject/StrongObjectPtr.h"

class UDataForgeAssetLayoutRecipe;
class UDataForgeBindingPreset;
class UDataForgeFolderSourceConfig;
class UDataForgeNamingPolicy;
class UDataForgeRuleSet;

struct DATAFORGEEDITOR_API FDataForgeAuthoringMaterializationRequest
{
	FDataForgeAuthoringPlannerResult Planned;
	TWeakObjectPtr<UDataForgeNamingPolicy> NamingPolicy;
	TWeakObjectPtr<UDataForgeFolderSourceConfig> ReusableFolderSource;
};

/** Owns a transient, reviewable draft. No package is created or saved by BuildDraft. */
struct DATAFORGEEDITOR_API FDataForgeAuthoringDraft
{
	bool bSuccess = false;
	TStrongObjectPtr<UDataForgeRuleSet> RuleSet;
	TArray<TStrongObjectPtr<UDataForgeBindingPreset>> BindingPresets;
	TArray<TStrongObjectPtr<UDataForgeAssetLayoutRecipe>> LayoutRecipes;
	TArray<TStrongObjectPtr<UDataForgeFolderSourceConfig>> FolderSources;
	TArray<FDataForgeDiagnostic> Diagnostics;
	FString Summary;
};

class DATAFORGEEDITOR_API FDataForgeAuthoringMaterializer
{
public:
	static FDataForgeAuthoringDraft BuildDraft(const FDataForgeAuthoringMaterializationRequest& Request);
};
