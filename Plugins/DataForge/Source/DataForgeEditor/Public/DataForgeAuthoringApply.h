#pragma once

#include "CoreMinimal.h"
#include "DataForgeAuthoringMaterializer.h"

class UDataForgeRuleSet;

struct DATAFORGEEDITOR_API FDataForgeAuthoringApplyRequest
{
	const FDataForgeAuthoringDraft* Draft = nullptr;
	/** Optional Wizard-created target. It must occupy RuleSetPath; existing-target Apply never saves immediately. */
	UDataForgeRuleSet* ExistingRuleSet = nullptr;
	FString RuleSetPath;
	FString DefinitionFolder;
	bool bSaveAssets = true;
};

struct DATAFORGEEDITOR_API FDataForgeAuthoringApplyResult
{
	bool bSuccess = false;
	TWeakObjectPtr<UDataForgeRuleSet> RuleSet;
	TArray<TWeakObjectPtr<UObject>> CreatedAssets;
	TArray<FString> CreatedObjectPaths;
	TArray<FDataForgeDiagnostic> Diagnostics;
	FString Summary;
};

/** Initial-creation-only promotion of a reviewed transient draft into persistent asset packages. */
class DATAFORGEEDITOR_API FDataForgeAuthoringApply
{
public:
	static FDataForgeAuthoringApplyResult Apply(const FDataForgeAuthoringApplyRequest& Request);
};
