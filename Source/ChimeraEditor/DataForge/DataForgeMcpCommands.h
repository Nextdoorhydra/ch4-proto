#pragma once

#include "CoreMinimal.h"
#include "DataForgeTypes.h"

class IConsoleObject;
class UDataForgeAssetLayoutProfile;

struct FDataForgeGoogleRuleSetRequest
{
	FString GoogleParserPath;
	FString RuleSetPath;
	FString RowStructPath;
	FString OutputDataTablePath;
	FName PrimaryKey = NAME_None;
	/** Exact Profile asset path. Mutually exclusive with AssetLayoutPurpose. */
	FString AssetLayoutProfilePath;
	/** Discovery key matched against Profile Purpose or Tags when no exact path is supplied. */
	FName AssetLayoutPurpose = NAME_None;
	TMap<FName, FString> AssetLayoutParameters;
	TArray<FDataForgeAssetRule> AssetRules;
	TArray<FDataForgeGeneratedAssetOutputRule> GeneratedOutputs;
	TArray<FDataForgeBindingRule> Bindings;
	bool bApply = true;
	bool bSaveAssets = true;
};

struct FDataForgeGoogleRuleSetResult
{
	bool bSuccess = false;
	FString Message;
	FString RuleSetObjectPath;
	FString OutputDataTableObjectPath;
	int32 DetectedColumnCount = 0;
	int32 BindingCount = 0;
	FString AssetLayoutProfileObjectPath;
};

namespace DataForgeMcpCommands
{
	bool ParseRequestSpec(
		const FString& Json,
		FDataForgeGoogleRuleSetRequest& OutRequest,
		FString& OutError);

	bool CreateRuleSetFromGoogleParser(
		const FDataForgeGoogleRuleSetRequest& Request,
		FDataForgeGoogleRuleSetResult& OutResult);

	/** Sorted exact candidates. Purpose matches either Profile.Purpose or one of its Tags. */
	void DiscoverAssetLayoutProfiles(FName Purpose, TArray<FString>& OutObjectPaths);

	/** Exact path resolves directly; purpose discovery succeeds only for exactly one candidate. */
	bool ResolveAssetLayoutProfile(
		const FDataForgeGoogleRuleSetRequest& Request,
		UDataForgeAssetLayoutProfile*& OutProfile,
		TArray<FString>& OutCandidates,
		FString& OutError);

	IConsoleObject* Register();
	void Unregister(IConsoleObject*& Command);
}
