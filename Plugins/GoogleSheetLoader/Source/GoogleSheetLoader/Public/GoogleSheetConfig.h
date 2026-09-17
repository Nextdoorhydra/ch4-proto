#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GoogleSheetParserBase.h"
#include "GoogleSheetConfig.generated.h"

class UGoogleSheetConfig;
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGoogleSheetCacheUpdated, UGoogleSheetConfig&);

UENUM()
enum class EFetchStatus : uint8
{
	None UMETA(DisplayName = "Idle"),
	Success UMETA(DisplayName = "Success"),
	Failed UMETA(DisplayName = "Failed"),
	Loading UMETA(DisplayName = "Loading")
};

UCLASS(BlueprintType)
class GOOGLESHEETLOADER_API UGoogleSheetConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Google Sheet|Config",
		meta = (DisplayName = "Google Sheet URL", ToolTip = "Public Google Sheets URL or spreadsheet id."))
	FString SheetURL;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Google Sheet|Config")
	FString RangeFrom = TEXT("A1");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Google Sheet|Config")
	FString RangeTo = TEXT("Z100");

	/** Optional legacy materializer. Leave empty and save normalized JSON for DataForge/cache-only use. */
	UPROPERTY(EditAnywhere, Instanced, Category = "Google Sheet|Config",
		meta = (ToolTip = "Optional. Leave empty and enable Save Normalized Json to fetch without assigning a DataTable."))
	TObjectPtr<UGoogleSheetParserBase> DataParser;

	UPROPERTY(EditAnywhere, Category = "Google Sheet|Config")
	bool bAutoSaveOnComplete = false;

	/** Save TSV converted to normalized JSON under Saved/GoogleSheetLoader. */
	UPROPERTY(EditAnywhere, Category = "Google Sheet|Config")
	bool bSaveNormalizedJson = true;

	/** Save the normalized cache without invoking DataParser. */
	UPROPERTY(EditAnywhere, Category = "Google Sheet|Config",
		meta = (EditCondition = "bSaveNormalizedJson", ToolTip = "Fetch only refreshes the normalized cache for DataForge."))
	bool bSkipDataParser = false;

	/** Notify DataForge after a successful cache refresh. Enabled by default for immediate RuleSet Preview and Apply. */
	UPROPERTY(EditAnywhere, Category = "Google Sheet|Automation",
		meta = (EditCondition = "bSaveNormalizedJson", ToolTip = "Immediately Preview and Apply every DataForge RuleSet that references this config after Fetch succeeds."))
	bool bAutoApplyDataForge = true;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Transient, Category = "Google Sheet|Status")
	EFetchStatus FetchStatus = EFetchStatus::None;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Google Sheet|Status")
	FString LastMessage;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Google Sheet|Status")
	FString LastFetchTime;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Google Sheet|Status")
	FString LastNormalizedJsonPath;
#endif

#if WITH_EDITOR
	static FOnGoogleSheetCacheUpdated& OnCacheUpdated();
	UGoogleSheetParserBase* GetActiveParser() const { return DataParser.Get(); }
	FString GetSpreadsheetID() const;
	FString GetRangeString() const;
	FString GetSheetGid() const;
	void Fetch();
#endif
};
