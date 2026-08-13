#pragma once

#include "CoreMinimal.h"
#include "DataForgeBindingPreset.h"
#include "Engine/EngineTypes.h"
#include "UObject/SoftObjectPtr.h"
#include "DataForgeTypes.generated.h"

class FStructOnScope;
class UDataAsset;
class UDataTable;
class UScriptStruct;

UENUM(BlueprintType)
enum class EDataForgeSeverity : uint8
{
	Info,
	Warning,
	Error
};

UENUM(BlueprintType)
enum class EDataForgeBindingSource : uint8
{
	SourceValue,
	ResolvedAsset,
	GeneratedOutput
};

UENUM(BlueprintType)
enum class EDataForgeBindingTarget : uint8
{
	DataTableRow,
	GeneratedOutput
};

UENUM(BlueprintType)
enum class EDataForgeAssetOwnership : uint8
{
	Managed,
	External
};

UENUM(BlueprintType)
enum class EDataForgeRowChange : uint8
{
	Create,
	Update,
	Unchanged,
	Orphan
};

UENUM(BlueprintType)
enum class EDataForgeGeneratedAssetType : uint8
{
	DataAsset,
	PrimaryDataAsset
};

UENUM(BlueprintType)
enum class EDataForgeManagedAssetChange : uint8
{
	Create,
	Move,
	Update,
	Unchanged,
	Orphan
};

/** One physical/provider input used by the Multi Source adapter. The first entry is the primary row set. */
USTRUCT(BlueprintType)
struct DATAFORGECORE_API FDataForgeSourceInput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source", meta = (ToolTip = "CSV, JSON, or Google Sheet Cache adapter. Multi Source cannot contain itself."))
	FName AdapterId = TEXT("Csv");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source", meta = (ToolTip = "CSV/JSON source file selected with the file browser."))
	FFilePath File;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source", meta = (ToolTip = "Provider asset used by adapters such as Google Sheet Cache."))
	TSoftObjectPtr<UObject> SourceAsset;

	/** Primary key for the first input; foreign-key column for every later input. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Join", meta = (ToolTip = "For the first input this is the primary join key. For later inputs it is the foreign-key column matched to the first input."))
	FName JoinColumn = NAME_None;

	/** Optional prefix applied to non-key columns, useful when sources use the same column names. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Join", meta = (ToolTip = "Optional prefix for joined non-key columns, for example Stats_ turns Price into Stats_Price."))
	FString ColumnPrefix;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source", AdvancedDisplay)
	TMap<FName, FString> Parameters;
};

USTRUCT(BlueprintType)
struct DATAFORGECORE_API FDataForgeSourceConfig
{
	GENERATED_BODY()

	/** Capability id resolved through FDataForgeSourceAdapterRegistry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source", meta = (ToolTip = "Source adapter selected from the registered adapter list. Use the editor dropdown instead of typing this id."))
	FName AdapterId = TEXT("Csv");

	/** Optional absolute path, or a path relative to the Unreal project directory. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source", meta = (ToolTip = "CSV/JSON source file. Use the file browser button; project-relative paths are supported."))
	FFilePath File;

	/** Optional provider/config asset consumed by adapters such as GoogleSheetCache. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source", meta = (ToolTip = "Optional provider asset. For GoogleSheetCache, select a GoogleSheetConfig asset here."))
	TSoftObjectPtr<UObject> SourceAsset;

	/** Used only by Multi Source. Entry 0 supplies the output rows; later entries are left-joined by JoinColumn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source", meta = (ToolTip = "Inputs for Multi Source. The first source defines rows; following sources join by their configured key."))
	TArray<FDataForgeSourceInput> Inputs;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source", AdvancedDisplay, meta = (ClampMin = "1", ClampMax = "1000", ToolTip = "Maximum sample rows read by Probe. Apply always reads the complete source."))
	int32 ProbeRowLimit = 20;

	/** Adapter-specific values. Custom adapters define the keys they consume. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source", AdvancedDisplay, meta = (ToolTip = "Adapter-specific options. Most built-in workflows do not require manual values."))
	TMap<FName, FString> Parameters;
};

/** Secondary parsed-data source used to associate one or more assets with each primary source row. */
USTRUCT(BlueprintType)
struct DATAFORGECORE_API FDataForgeAssociationSourceRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Association")
	FName SourceId = NAME_None;

	/** Any registered adapter is valid; the result is consumed as canonical Parsed Data. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Association")
	FDataForgeSourceConfig Source;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Columns")
	FName MatchColumn = TEXT("Subject");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Columns")
	FName AssetPathColumn = TEXT("ObjectPath");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Columns")
	FName AssetKindColumn = TEXT("AssetKind");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Columns")
	FName RoleColumn = TEXT("Role");
};

USTRUCT(BlueprintType)
struct DATAFORGECORE_API FDataForgeSchemaRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Schema", meta = (ToolTip = "Unique source column used as the DataTable row name. Probe suggests a key automatically."))
	FName PrimaryKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Schema", AdvancedDisplay, meta = (ToolTip = "Columns that must exist. An empty list is populated from the first successful Probe."))
	TArray<FName> RequiredColumns;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Schema", AdvancedDisplay, meta = (ToolTip = "Report source columns that are not mapped to the selected row struct."))
	bool bWarnOnUnmappedColumns = true;
};

USTRUCT(BlueprintType)
struct DATAFORGECORE_API FDataForgeDataTableOutputRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output", meta = (ToolTip = "DataTable row USTRUCT. It must derive from FTableRowBase."))
	TObjectPtr<UScriptStruct> RowStruct = nullptr;

	/** Long package path, for example /Game/GameData/Tables/DT_ItemCatalog. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output", meta = (ToolTip = "Content Browser package path for the generated or updated DataTable, for example /Game/Data/DT_Items."))
	FString AssetPath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output", AdvancedDisplay)
	bool bCreateIfMissing = true;

	/** Missing source records remain in the table unless this is explicitly enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output", AdvancedDisplay)
	bool bRemoveRowsMissingFromSource = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output", AdvancedDisplay)
	bool bSaveAfterApply = true;
};

USTRUCT(BlueprintType)
struct DATAFORGECORE_API FDataForgeAssetRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset Rule")
	FName RuleId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset Rule")
	EDataForgeAssetOwnership Ownership = EDataForgeAssetOwnership::External;

	/** Long package folder such as /Game/Art/Items. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset Rule", meta = (ToolTip = "Content Browser base folder used to resolve or create assets."))
	FString BaseFolder;

	/** Optional token pattern such as {Category}/{ItemId}/Textures. Tokens are exact source column names. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset Rule", meta = (ToolTip = "Optional subfolder pattern. Each {ColumnName} is replaced with that row's source value; token names are case-sensitive."))
	FString SubfolderPattern;

	/** Asset name pattern such as T_{ItemId}_Icon. Tokens are exact source column names. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset Rule", meta = (ToolTip = "Asset name pattern. Each {ColumnName} is replaced with that row's source value. Unknown or incomplete tokens fail Preview."))
	FString AssetNamePattern;
};

USTRUCT(BlueprintType)
struct DATAFORGECORE_API FDataForgeGeneratedAssetOutputRule
{
	GENERATED_BODY()

	/** Stable name used by bindings, for example data. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generated Output")
	FName OutputName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generated Output")
	EDataForgeGeneratedAssetType Type = EDataForgeGeneratedAssetType::DataAsset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generated Output")
	TSubclassOf<UDataAsset> AssetClass;

	/** Must reference an Asset Rule whose ownership is Managed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generated Output")
	FName AssetRuleId = NAME_None;
};

USTRUCT(BlueprintType)
struct DATAFORGECORE_API FDataForgeBindingRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Binding")
	EDataForgeBindingSource Source = EDataForgeBindingSource::SourceValue;

	/** Canonical source field used as the value, or as input to an external asset rule. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Binding", meta = (EditCondition = "Source != EDataForgeBindingSource::GeneratedOutput", EditConditionHides))
	FName SourceColumn = NAME_None;

	/** Generated output used as the value, such as data -> row.DataAsset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Binding", meta = (EditCondition = "Source == EDataForgeBindingSource::GeneratedOutput", EditConditionHides))
	FName SourceOutput = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Binding")
	EDataForgeBindingTarget Target = EDataForgeBindingTarget::DataTableRow;

	/** Required when Target is GeneratedOutput. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Binding", meta = (EditCondition = "Target == EDataForgeBindingTarget::GeneratedOutput", EditConditionHides))
	FName TargetOutput = NAME_None;

	/** Dot-separated property path. A leading "row." is optional for row targets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Binding")
	FString TargetProperty;

	/** Required when Source is ResolvedAsset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Binding", meta = (EditCondition = "Source == EDataForgeBindingSource::ResolvedAsset", EditConditionHides))
	FName AssetRuleId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Binding")
	bool bRequired = true;
};

USTRUCT(BlueprintType)
struct DATAFORGECORE_API FDataForgeDiagnostic
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostic")
	EDataForgeSeverity Severity = EDataForgeSeverity::Info;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostic")
	FName RecordId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostic")
	FName Field = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostic")
	int32 SourceRow = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostic")
	FString Code;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostic")
	FString Message;
};

struct DATAFORGECORE_API FDataForgeRow
{
	TMap<FName, FString> Values;
	int32 SourceRow = INDEX_NONE;
};

struct DATAFORGECORE_API FDataForgeDataSet
{
	TArray<FName> Columns;
	TArray<FDataForgeRow> Rows;
	FString SourceRevision;
};

struct DATAFORGECORE_API FDataForgeCompiledBinding
{
	FDataForgeBindingRule Rule;
	FProperty* TargetProperty = nullptr;
	TArray<FProperty*> PropertyChain;
};

struct DATAFORGECORE_API FDataForgeCompiledOutput
{
	FDataForgeGeneratedAssetOutputRule Rule;
	TWeakObjectPtr<UClass> AssetClass;
};

struct DATAFORGECORE_API FDataForgeCompiledAssociationSlot
{
	FName OutputName = NAME_None;
	FName SlotId = NAME_None;
	FName AssociationSourceId = NAME_None;
	FName SourceKeyColumn = NAME_None;
	FName AssetKind = NAME_None;
	FName Role = NAME_None;
	FString TargetPropertyPath;
	EDataForgeBindingCardinality Cardinality = EDataForgeBindingCardinality::One;
	EDataForgeBindingReconcileMode Reconcile = EDataForgeBindingReconcileMode::Assign;
	bool bRequired = true;
	FProperty* TargetProperty = nullptr;
	TArray<FProperty*> PropertyChain;
};

struct DATAFORGECORE_API FCompiledDataForgeRuleSet
{
	TWeakObjectPtr<const class UDataForgeRuleSet> RuleSet;
	TWeakObjectPtr<UScriptStruct> RowStruct;
	TArray<FDataForgeCompiledBinding> Bindings;
	TMap<FName, FDataForgeAssetRule> AssetRules;
	TMap<FName, FDataForgeCompiledOutput> GeneratedOutputs;
	TMap<FName, FDataForgeAssociationSourceRule> AssociationSourceRules;
	TMap<FName, FDataForgeDataSet> AssociationDataSets;
	TArray<FDataForgeCompiledAssociationSlot> AssociationSlots;
	FDataForgeDataSet DataSet;
};

struct DATAFORGECORE_API FDataForgePlannedRow
{
	FName RowName = NAME_None;
	EDataForgeRowChange Change = EDataForgeRowChange::Unchanged;
	TSharedPtr<FStructOnScope> DesiredData;
};

struct DATAFORGECORE_API FDataForgePlannedPropertyWrite
{
	FString PropertyPath;
	FString PreviousValue;
	FString ExportedValue;
};

struct DATAFORGECORE_API FDataForgePlannedAsset
{
	FName RecordId = NAME_None;
	FName OutputName = NAME_None;
	FString PreviousPackageName;
	FString PreviousObjectPath;
	FString PackageName;
	FString ObjectPath;
	TWeakObjectPtr<UClass> AssetClass;
	TWeakObjectPtr<UDataAsset> ExistingAsset;
	EDataForgeManagedAssetChange Change = EDataForgeManagedAssetChange::Unchanged;
	TArray<FDataForgePlannedPropertyWrite> PropertyWrites;
	/** Property path -> sorted paths owned by the Association Manifest after Apply. */
	TMap<FString, TArray<FString>> ManagedAssociations;
	/** Removed slots preserve their current property value as manual data and drop only stale ownership metadata. */
	TArray<FString> RemovedAssociationKeys;
};

struct DATAFORGECORE_API FDataForgeApplyPlan
{
	TWeakObjectPtr<const class UDataForgeRuleSet> RuleSet;
	TWeakObjectPtr<UDataTable> ExistingTable;
	TArray<FDataForgePlannedRow> Rows;
	TArray<FDataForgePlannedAsset> ManagedAssets;
	TArray<FDataForgeDiagnostic> Diagnostics;
	FString SourceRevision;
	FString TargetRevision;
	int32 CreateCount = 0;
	int32 UpdateCount = 0;
	int32 UnchangedCount = 0;
	int32 OrphanCount = 0;
	int32 AssetCreateCount = 0;
	int32 AssetMoveCount = 0;
	int32 AssetUpdateCount = 0;
	int32 AssetUnchangedCount = 0;
	int32 AssetOrphanCount = 0;

	bool HasErrors() const;
	FString MakeSummary() const;
};

struct DATAFORGECORE_API FDataForgeResult
{
	bool bSuccess = false;
	TArray<FDataForgeDiagnostic> Diagnostics;
	FString Summary;
};
