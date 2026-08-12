#include "DataForgeTypes.h"

bool FDataForgeApplyPlan::HasErrors() const
{
	return Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Severity == EDataForgeSeverity::Error;
	});
}

FString FDataForgeApplyPlan::MakeSummary() const
{
	int32 ErrorCount = 0;
	int32 WarningCount = 0;
	for (const FDataForgeDiagnostic& Diagnostic : Diagnostics)
	{
		ErrorCount += Diagnostic.Severity == EDataForgeSeverity::Error ? 1 : 0;
		WarningCount += Diagnostic.Severity == EDataForgeSeverity::Warning ? 1 : 0;
	}

	return FString::Printf(
		TEXT("Rows C:%d U:%d =:%d O:%d | Assets C:%d M:%d U:%d =:%d O:%d | Errors:%d Warnings:%d"),
		CreateCount,
		UpdateCount,
		UnchangedCount,
		OrphanCount,
		AssetCreateCount,
		AssetMoveCount,
		AssetUpdateCount,
		AssetUnchangedCount,
		AssetOrphanCount,
		ErrorCount,
		WarningCount);
}
