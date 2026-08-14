#pragma once

#include "CoreMinimal.h"
#include "DataForgeAuthoringMaterializer.h"

struct FDataForgeApplyPlan;
class UDataForgeRuleSet;

struct DATAFORGEEDITOR_API FDataForgeAuthoringReviewEntry
{
	FString Label;
	FString Value;
	TArray<FString> Details;
};

struct DATAFORGEEDITOR_API FDataForgeAuthoringReviewSection
{
	FString Title;
	TArray<FDataForgeAuthoringReviewEntry> Entries;
};

/** Read-only representation of inferred authoring decisions and their expected effects. */
struct DATAFORGEEDITOR_API FDataForgeAuthoringReview
{
	TArray<FDataForgeAuthoringReviewSection> Sections;
	TArray<FDataForgeDiagnostic> Diagnostics;

	FString ToDisplayString() const;
};

class DATAFORGEEDITOR_API FDataForgeAuthoringReviewBuilder
{
public:
	static FDataForgeAuthoringReview Build(
		const FDataForgeAuthoringPlannerResult& Planned,
		const FDataForgeAuthoringDraft& Draft,
		const UDataForgeRuleSet& ConfiguredRuleSet,
		const FString& RuleSetPath,
		const FString& DefinitionFolder,
		const FDataForgeApplyPlan* PreviewPlan = nullptr);
};
