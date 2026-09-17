#pragma once

#include "CoreMinimal.h"
#include "DataForgeTypes.h"
#include "Engine/DataAsset.h"
#include "UObject/SoftObjectPtr.h"
#include "DataForgeAssetLayoutProfile.generated.h"

UENUM(BlueprintType)
enum class EDataForgeProfileParameterType : uint8
{
	String,
	Name,
	ContentPath
};

USTRUCT(BlueprintType)
struct DATAFORGECORE_API FDataForgeProfileParameter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameter")
	FName Name = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameter")
	EDataForgeProfileParameterType Type = EDataForgeProfileParameterType::String;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameter")
	FString DefaultValue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameter")
	bool bRequired = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameter", meta = (MultiLine = true))
	FString Description;
};

USTRUCT(BlueprintType)
struct DATAFORGECORE_API FDataForgeLayoutRoot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout Root")
	FName RootId = NAME_None;

	/** Content path pattern such as /Game/${FeatureRoot}/Characters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout Root", meta = (ToolTip = "Content path pattern. Profile parameters use ${Name}; source columns use {ColumnName}."))
	FString PathPattern;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout Root", meta = (MultiLine = true))
	FString Description;
};

USTRUCT(BlueprintType)
struct DATAFORGECORE_API FDataForgeAssetRuleTemplate
{
	GENERATED_BODY()

	/** Stable template identity used to track a materialized rule across renames. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset Rule Template")
	FGuid TemplateId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset Rule Template")
	FName RuleId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset Rule Template")
	EDataForgeAssetOwnership Ownership = EDataForgeAssetOwnership::External;

	/** Authoring hint only; the compiler still derives target types from bindings and outputs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset Rule Template")
	TSoftClassPtr<UObject> ExpectedAssetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset Rule Template", meta = (ToolTip = "Folder below the selected root and group. Supports ${Parameter} and {SourceColumn} tokens."))
	FString RelativeFolderPattern;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset Rule Template", meta = (ToolTip = "Asset name pattern. Supports ${Parameter} and {SourceColumn} tokens."))
	FString AssetNamePattern;
};

USTRUCT(BlueprintType)
struct DATAFORGECORE_API FDataForgeAssetRuleGroupTemplate
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset Rule Group")
	FGuid TemplateId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset Rule Group")
	FName GroupId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset Rule Group")
	FName RootId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset Rule Group", meta = (ToolTip = "Optional folder below the selected root. Supports ${Parameter} and {SourceColumn} tokens."))
	FString GroupSubfolderPattern;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Asset Rule Group")
	TArray<FDataForgeAssetRuleTemplate> Rules;
};

UCLASS(BlueprintType)
class DATAFORGECORE_API UDataForgeAssetLayoutProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DataForge")
	FGuid ProfileId;

	/** Human-authored release number. Layout freshness uses the deterministic revision hash. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DataForge", meta = (ClampMin = "1"))
	int32 ProfileVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Discovery")
	FName Purpose = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Discovery")
	TArray<FName> Tags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TArray<FDataForgeLayoutRoot> Roots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TArray<FDataForgeProfileParameter> Parameters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TArray<FDataForgeAssetRuleGroupTemplate> Groups;
};

USTRUCT(BlueprintType)
struct DATAFORGECORE_API FDataForgeMaterializedRuleOrigin
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile Origin")
	FGuid GroupTemplateId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile Origin")
	FGuid RuleTemplateId;

	/** Typed baseline used to derive future override status without persisting per-field state. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile Origin")
	FDataForgeAssetRule BaselineRule;
};

USTRUCT(BlueprintType)
struct DATAFORGECORE_API FDataForgeProfileOrigin
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile Origin")
	TSoftObjectPtr<UDataForgeAssetLayoutProfile> Profile;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile Origin")
	FGuid ProfileId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile Origin")
	int32 MaterializedVersion = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile Origin")
	FString MaterializedHash;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile Origin")
	TMap<FName, FString> ParameterValues;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile Origin")
	TArray<FDataForgeMaterializedRuleOrigin> Rules;

	bool IsSet() const { return ProfileId.IsValid(); }
};

struct DATAFORGECORE_API FDataForgeAssetLayoutMaterialization
{
	bool bSuccess = false;
	TArray<FDataForgeAssetRule> AssetRules;
	FDataForgeProfileOrigin Origin;
	TArray<FDataForgeDiagnostic> Diagnostics;
};

/** Pure authoring service that validates and expands a Profile into concrete RuleSet asset rules. */
class DATAFORGECORE_API FDataForgeAssetLayoutMaterializer
{
public:
	static FString ComputeLayoutRevisionHash(const UDataForgeAssetLayoutProfile& Profile);

	/** Validate Profile structure without requiring authoring-time values for required parameters. */
	static FDataForgeResult ValidateDefinition(const UDataForgeAssetLayoutProfile& Profile);

	/** SourceColumns is optional. When supplied, every {Column} token must match it exactly. */
	static FDataForgeAssetLayoutMaterialization Materialize(
		const UDataForgeAssetLayoutProfile& Profile,
		const TMap<FName, FString>& ParameterValues,
		const TArray<FName>* SourceColumns = nullptr);
};
