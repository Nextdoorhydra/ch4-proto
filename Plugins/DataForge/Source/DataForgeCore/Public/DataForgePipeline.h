#pragma once

#include "CoreMinimal.h"
#include "DataForgeTypes.h"

class UDataForgeRuleSet;

struct DATAFORGECORE_API FDataForgeSourceDescriptor
{
	FName AdapterId = NAME_None;
	FText DisplayName;
	FString Description;
	FString FileExtension;
};

class DATAFORGECORE_API IDataForgeSourceAdapter
{
public:
	virtual ~IDataForgeSourceAdapter() = default;
	virtual FDataForgeSourceDescriptor Describe() const = 0;

	virtual bool Probe(
		const FDataForgeSourceConfig& Source,
		FDataForgeDataSet& OutDataSet,
		TArray<FDataForgeDiagnostic>& OutDiagnostics) const = 0;

	virtual bool Fetch(
		const FDataForgeSourceConfig& Source,
		FDataForgeDataSet& OutDataSet,
		TArray<FDataForgeDiagnostic>& OutDiagnostics) const = 0;
};

class DATAFORGECORE_API FDataForgeCsvSourceAdapter final : public IDataForgeSourceAdapter
{
public:
	virtual FDataForgeSourceDescriptor Describe() const override;

	virtual bool Probe(
		const FDataForgeSourceConfig& Source,
		FDataForgeDataSet& OutDataSet,
		TArray<FDataForgeDiagnostic>& OutDiagnostics) const override;

	virtual bool Fetch(
		const FDataForgeSourceConfig& Source,
		FDataForgeDataSet& OutDataSet,
		TArray<FDataForgeDiagnostic>& OutDiagnostics) const override;

	/** Public for deterministic automation tests and adapters that already own CSV text. */
	static bool Parse(
		const FString& CsvText,
		int32 RowLimit,
		FDataForgeDataSet& OutDataSet,
		TArray<FDataForgeDiagnostic>& OutDiagnostics);

private:
	static bool Read(
		const FDataForgeSourceConfig& Source,
		int32 RowLimit,
		FDataForgeDataSet& OutDataSet,
		TArray<FDataForgeDiagnostic>& OutDiagnostics);
};

class DATAFORGECORE_API FDataForgeJsonSourceAdapter final : public IDataForgeSourceAdapter
{
public:
	virtual FDataForgeSourceDescriptor Describe() const override;

	virtual bool Probe(
		const FDataForgeSourceConfig& Source,
		FDataForgeDataSet& OutDataSet,
		TArray<FDataForgeDiagnostic>& OutDiagnostics) const override;

	virtual bool Fetch(
		const FDataForgeSourceConfig& Source,
		FDataForgeDataSet& OutDataSet,
		TArray<FDataForgeDiagnostic>& OutDiagnostics) const override;

	static bool Parse(
		const FString& JsonText,
		int32 RowLimit,
		FDataForgeDataSet& OutDataSet,
		TArray<FDataForgeDiagnostic>& OutDiagnostics);

private:
	static bool Read(
		const FDataForgeSourceConfig& Source,
		int32 RowLimit,
		FDataForgeDataSet& OutDataSet,
		TArray<FDataForgeDiagnostic>& OutDiagnostics);
};

/** Process-wide extension point. Adapters normalize any parser/provider output into FDataForgeDataSet. */
class DATAFORGECORE_API FDataForgeSourceAdapterRegistry
{
public:
	static FDataForgeSourceAdapterRegistry& Get();

	bool Register(const TSharedRef<IDataForgeSourceAdapter>& Adapter);
	void Unregister(FName AdapterId);
	TSharedPtr<const IDataForgeSourceAdapter> Find(FName AdapterId) const;
	TArray<FDataForgeSourceDescriptor> DescribeAll() const;

private:
	TMap<FName, TSharedPtr<IDataForgeSourceAdapter>> Adapters;
};

class DATAFORGECORE_API FDataForgeCompiler
{
public:
	static bool Probe(
		const UDataForgeRuleSet& RuleSet,
		FDataForgeDataSet& OutDataSet,
		TArray<FDataForgeDiagnostic>& OutDiagnostics);

	static bool Compile(
		const UDataForgeRuleSet& RuleSet,
		FCompiledDataForgeRuleSet& OutCompiled,
		TArray<FDataForgeDiagnostic>& OutDiagnostics);

	static FDataForgeApplyPlan BuildPlan(const FCompiledDataForgeRuleSet& Compiled);

	static bool IsManagedAssetOwnedBy(
		const UObject& Asset,
		const UDataForgeRuleSet& RuleSet,
		FName OutputName,
		FName RecordId);

	static bool ApplyPlannedProperties(
		UObject& Target,
		const FDataForgePlannedAsset& PlannedAsset,
		TArray<FDataForgeDiagnostic>& OutDiagnostics);
};
