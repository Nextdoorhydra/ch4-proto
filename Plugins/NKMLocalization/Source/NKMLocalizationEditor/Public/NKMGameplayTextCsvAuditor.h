#pragma once

#include "CoreMinimal.h"
#include "NKMLocalizationTypes.h"

struct FNKMGameplayTextCsvIssue
{
	int32 Row = 0;
	FString Column;
	FString Value;
	FString Code;
	FString Message;
};

struct FNKMGameplayTextCsvAuditReport
{
	int32 CheckedReferenceCount = 0;
	TArray<FNKMGameplayTextCsvIssue> Issues;
	bool bSucceeded = false;

	bool Succeeded() const { return bSucceeded; }
};

class NKMLOCALIZATIONEDITOR_API FNKMGameplayTextCsvAuditor
{
public:
	static bool AuditFile(
		const FString& CsvFilename,
		const FNKMLocalizationDocument& SourceDocument,
		FName DefaultTableId,
		const TArray<FString>& TextColumns,
		FNKMGameplayTextCsvAuditReport& OutReport,
		FNKMLocalizationResult& OutResult);

	static TArray<FString> DetectTextColumns(const TArray<FString>& Headers);
	static bool WriteReport(const FString& Filename, const FNKMGameplayTextCsvAuditReport& Report);
};
