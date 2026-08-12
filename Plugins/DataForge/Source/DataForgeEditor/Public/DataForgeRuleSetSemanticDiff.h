#pragma once

#include "CoreMinimal.h"

class UDataForgeRuleSet;

enum class EDataForgeSemanticDiffKind : uint8
{
	Added,
	Removed,
	Modified
};

struct DATAFORGEEDITOR_API FDataForgeSemanticDiffEntry
{
	EDataForgeSemanticDiffKind Kind = EDataForgeSemanticDiffKind::Modified;
	FString Path;
	FString BeforeValue;
	FString AfterValue;

	FString ToDisplayString() const;
};

class DATAFORGEEDITOR_API FDataForgeRuleSetSemanticDiff
{
public:
	static TArray<FDataForgeSemanticDiffEntry> Compare(
		const UDataForgeRuleSet& Before,
		const UDataForgeRuleSet& After);
};
