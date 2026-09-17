#pragma once

#include "CoreMinimal.h"
#include "DataForgeTypes.h"

class UDataForgeRuleSet;

struct DATAFORGEEDITOR_API FDataForgeRenameRebind
{
	FName RecordId = NAME_None;
	FName OutputName = NAME_None;
	FString PropertyPath;
	FString OldObjectPath;
	FString NewObjectPath;
};

struct DATAFORGEEDITOR_API FDataForgeRenameImpactEntry
{
	TWeakObjectPtr<const UDataForgeRuleSet> RuleSet;
	bool bSuccess = false;
	TArray<FDataForgeRenameRebind> Rebinds;
	TArray<FDataForgeDiagnostic> Diagnostics;

	FString MakeSummary() const;
};

struct DATAFORGEEDITOR_API FDataForgeRenameImpact
{
	TArray<FDataForgeRenameImpactEntry> Entries;

	bool IsSafe() const;
	FString MakeSummary() const;
};

/** Mutation-free preview used by automatic rename handling and the future rename candidate UI. */
class DATAFORGEEDITOR_API FDataForgeRenameImpactAnalyzer
{
public:
	static FDataForgeRenameImpactEntry AnalyzeRuleSet(
		const UDataForgeRuleSet& RuleSet,
		const FSoftObjectPath& OldObjectPath,
		const FSoftObjectPath& NewObjectPath);

	static FDataForgeRenameImpact AnalyzeProject(
		const FSoftObjectPath& OldObjectPath,
		const FSoftObjectPath& NewObjectPath);
};
