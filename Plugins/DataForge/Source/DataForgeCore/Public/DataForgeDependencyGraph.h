#pragma once

#include "CoreMinimal.h"
#include "DataForgeTypes.h"

class UDataForgeRuleSet;

/** Resolves RuleSet prerequisites into a deterministic dependency-first execution order. */
class DATAFORGECORE_API FDataForgeDependencyGraph
{
public:
	static bool BuildExecutionOrder(
		TConstArrayView<const UDataForgeRuleSet*> Roots,
		TArray<const UDataForgeRuleSet*>& OutExecutionOrder,
		TArray<FDataForgeDiagnostic>& OutDiagnostics);
};
