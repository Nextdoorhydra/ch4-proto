#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPath.h"
#include "NKMLocalizationSettings.generated.h"

struct FNKMTextRef;

USTRUCT(BlueprintType)
struct NKMLOCALIZATION_API FNKMStringTableAliasOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Localization")
	FName Alias;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Localization", meta=(AllowedClasses="/Script/Engine.StringTable"))
	FSoftObjectPath TableAsset;
};

USTRUCT(BlueprintType)
struct NKMLOCALIZATION_API FNKMTextBindingField
{
	GENERATED_BODY()

	/** Logical localized field name used by {Field} in the profile key pattern. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Binding")
	FName FieldName;

	/** Optional legacy row property used only by the one-time JSON migration tool. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Binding")
	FName LegacySourceProperty;
};

USTRUCT(BlueprintType)
struct NKMLOCALIZATION_API FNKMTextBindingProfile
{
	GENERATED_BODY()

	/** Stable profile name referenced by gameplay parsers. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Binding")
	FName ProfileName;

	/** String Table alias receiving this profile's generated keys. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Binding")
	FName TableId;

	/** Supports {Id} and {Field}; for example Item.{Id}.{Field}. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Binding")
	FString KeyPattern = TEXT("{Id}.{Field}");

	/** Structured JSON filename relative to Authoring Source Root. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Migration")
	FString AuthoringSourceFile;

	/** Stable ID column used by the generic CSV record provider. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Record Provider")
	FName RecordIdColumn;

	/** Optional project-relative CSV used by Reconcile/Verify. An explicit command line CSV overrides it. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Record Provider", meta=(RelativeToGameDir))
	FFilePath RecordSourceFile;

	/** Optional asset containing a reflected record provider object. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Record Provider", meta=(AllowedClasses="/Script/CoreUObject.Object"))
	FSoftObjectPath RecordSourceAsset;

	/** Optional UObject property on Record Source Asset. Empty means the asset itself owns the map. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Record Provider")
	FName RecordSourceObjectProperty;

	/** TMap property containing records on the provider object. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Record Provider")
	FName RecordMapProperty;

	/** Stable ID property on each reflected record. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Record Provider")
	FName RecordIdProperty;

	/** Optional data asset containing the legacy parser object for one-time migration. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Migration", meta=(AllowedClasses="/Script/CoreUObject.Object"))
	FSoftObjectPath LegacySourceAsset;

	/** Object property on Legacy Source Asset that owns the parser UObject. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Migration")
	FName LegacyParserProperty = TEXT("DataParser");

	/** TMap property on the parser containing gameplay rows. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Migration")
	FName LegacyRowMapProperty;

	/** Stable ID property on each legacy row. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Migration")
	FName LegacyRecordIdProperty;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Binding")
	TArray<FNKMTextBindingField> Fields;
};

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="NKM Localization"))
class NKMLOCALIZATION_API UNKMLocalizationSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Unreal localization target name and generated artifact stem. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Identity")
	FString LocalizationTargetName = TEXT("NKMText");

	/** Optional segmented table alias prefix. For example NKM allows NKM.Items. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Identity")
	FString TableIdPrefix = TEXT("NKM");

	/** Prefix applied to the full table alias when deriving an asset name. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Identity")
	FString StringTableAssetPrefix = TEXT("ST_");

	/** Native source culture. Keep it first in Supported Cultures. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Cultures")
	FString NativeCulture = TEXT("ko");

	/** Cultures generated, compiled, staged, and exposed by the NKM dashboard. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Cultures")
	TArray<FString> SupportedCultures = {
		TEXT("ko"), TEXT("en"), TEXT("ar"), TEXT("fr"), TEXT("zh-Hans"), TEXT("de"), TEXT("it"),
		TEXT("ja"), TEXT("pt-BR"), TEXT("pl"), TEXT("ru"), TEXT("es-419"), TEXT("es"), TEXT("tr")};

	/** Root package path containing generated String Table assets. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Paths", meta=(ContentDir))
	FDirectoryPath StringTableAssetRoot = {TEXT("/Game/NetKarma/Localization/StringTables")};

	/**
	 * Project-relative directory containing authoring JSON files.
	 * This is fixed to Content/Localization/Source so it shares Unreal's standard localization layout.
	 */
	UPROPERTY(VisibleAnywhere, Config, BlueprintReadOnly, Category="Paths", meta=(RelativeToGameDir))
	FDirectoryPath AuthoringSourceRoot = {TEXT("Content/Localization/Source")};

	/**
	 * Project-relative manifest/archive/PO/LocRes output directory.
	 * This is fixed to Content/Localization/{LocalizationTargetName}, which Unreal's Localization Dashboard owns.
	 */
	UPROPERTY(VisibleAnywhere, Config, BlueprintReadOnly, Category="Paths", meta=(RelativeToGameDir))
	FDirectoryPath LocalizationTargetRoot = {TEXT("Content/Localization/NKMText")};

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="String Tables")
	TArray<FNKMStringTableAliasOverride> TableAliasOverrides;

	/** Central Stable ID + field to String Table key binding rules. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Bindings")
	TArray<FNKMTextBindingProfile> TextBindingProfiles;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	bool TryResolveOverride(FName Alias, FName& OutTableId) const;
	const FNKMTextBindingProfile* FindTextBindingProfile(FName ProfileName) const;
	bool TryMakeTextReference(FName ProfileName, FName RecordId, FName FieldName, FNKMTextRef& OutReference) const;
	FText ResolveBoundText(FName ProfileName, FName RecordId, FName FieldName) const;
	bool IsAllowedTableId(const FString& TableId) const;
	FString GetNormalizedStringTableAssetRoot() const;
	FString GetNormalizedAuthoringSourceRoot() const;
	FString GetNormalizedLocalizationTargetRoot() const;
	FString MakeConventionalTablePackagePath(FName Alias) const;
	FString MakeConventionalTableObjectPath(FName Alias) const;

	const FString& GetAppliedStringTableAssetRoot() const { return AppliedStringTableAssetRoot; }
	void SetAppliedStringTableAssetRoot(FString Value) { AppliedStringTableAssetRoot = MoveTemp(Value); }
	const FString& GetAppliedLocalizationTargetRoot() const { return AppliedLocalizationTargetRoot; }
	void SetAppliedLocalizationTargetRoot(FString Value) { AppliedLocalizationTargetRoot = MoveTemp(Value); }
	const FString& GetAppliedLocalizationTargetName() const { return AppliedLocalizationTargetName; }
	void SetAppliedLocalizationTargetName(FString Value) { AppliedLocalizationTargetName = MoveTemp(Value); }

private:
	/** Last root propagated to cook/gather config by the Editor path configurator. */
	UPROPERTY(Config)
	FString AppliedStringTableAssetRoot = TEXT("/Game/NetKarma/Localization/StringTables");

	UPROPERTY(Config)
	FString AppliedLocalizationTargetRoot = TEXT("Content/Localization/NKMText");

	UPROPERTY(Config)
	FString AppliedLocalizationTargetName = TEXT("NKMText");
};
