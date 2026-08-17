#include "NKMLocalizationTypes.h"

FString FNKMLocalizationDiagnostic::ToString() const
{
	FString Location;
	if (!TableId.IsEmpty())
	{
		Location = TableId;
		if (!Key.IsEmpty())
		{
			Location += TEXT("::") + Key;
		}
		Location = FString::Printf(TEXT(" [%s]"), *Location);
	}

	return FString::Printf(TEXT("%s%s: %s"), *Code, *Location, *Message);
}

void FNKMLocalizationResult::AddError(
	const FString& Code,
	const FString& Message,
	const FString& TableId,
	const FString& Key)
{
	Diagnostics.Add({ENKMLocalizationDiagnosticSeverity::Error, Code, Message, TableId, Key});
}

void FNKMLocalizationResult::AddWarning(
	const FString& Code,
	const FString& Message,
	const FString& TableId,
	const FString& Key)
{
	Diagnostics.Add({ENKMLocalizationDiagnosticSeverity::Warning, Code, Message, TableId, Key});
}

bool FNKMLocalizationResult::HasErrors() const
{
	return Diagnostics.ContainsByPredicate(
		[](const FNKMLocalizationDiagnostic& Diagnostic)
		{
			return Diagnostic.Severity == ENKMLocalizationDiagnosticSeverity::Error;
		});
}
