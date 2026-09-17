#pragma once

#include "CoreMinimal.h"
#include "DataForgeTypes.h"

class UDataForgeRuleSet;

/** Deterministic, review-only projection of a RuleSet asset for source control and CI. */
class DATAFORGEEDITOR_API FDataForgeRuleSetSnapshot
{
public:
	static FString SerializeJson(const UDataForgeRuleSet& RuleSet);
	static FString SerializeYaml(const UDataForgeRuleSet& RuleSet);

	/** Empty RootDirectory uses Config/DataForge/Snapshots. */
	static FDataForgeResult Export(
		const UDataForgeRuleSet& RuleSet,
		const FString& RootDirectory = FString(),
		FString* OutJsonFilename = nullptr,
		FString* OutYamlFilename = nullptr);

	/** Verifies that both checked-in snapshot formats exactly match the normalized RuleSet. */
	static FDataForgeResult Verify(
		const UDataForgeRuleSet& RuleSet,
		const FString& RootDirectory = FString());

	static void GetFilenames(
		const UDataForgeRuleSet& RuleSet,
		const FString& RootDirectory,
		FString& OutJsonFilename,
		FString& OutYamlFilename);
};
