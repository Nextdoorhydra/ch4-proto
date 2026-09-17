#pragma once

#include "CoreMinimal.h"

class FJsonValue;

enum class ENKMLocalizationDiagnosticSeverity : uint8
{
	Warning,
	Error
};

struct FNKMLocalizationDiagnostic
{
	ENKMLocalizationDiagnosticSeverity Severity = ENKMLocalizationDiagnosticSeverity::Error;
	FString Code;
	FString Message;
	FString TableId;
	FString Key;

	FString ToString() const;
};

struct FNKMLocalizationResult
{
	TArray<FNKMLocalizationDiagnostic> Diagnostics;

	void AddError(const FString& Code, const FString& Message, const FString& TableId = FString(), const FString& Key = FString());
	void AddWarning(const FString& Code, const FString& Message, const FString& TableId = FString(), const FString& Key = FString());
	bool HasErrors() const;
};

struct FNKMLocalizationEntry
{
	FString Key;
	FString SourceString;
	TMap<FName, FString> MetaData;
};

struct FNKMLocalizationTable
{
	FString Id;
	FString AssetPath;
	TArray<FNKMLocalizationEntry> Entries;
};

struct FNKMLocalizationDocument
{
	int32 SchemaVersion = 1;
	FString Target;
	FString NativeCulture;
	TArray<FNKMLocalizationTable> Tables;
	/** Schema-owned key lifecycle records. The editor preserves their JSON shape verbatim. */
	TArray<TSharedPtr<FJsonValue>> Redirects;
};

struct FNKMLocalizationSourceContext
{
	FString Target;
	FString NativeCulture;
	FString TableId;
	FString AssetPath;
};
