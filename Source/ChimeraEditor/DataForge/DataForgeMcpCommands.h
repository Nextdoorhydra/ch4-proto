#pragma once

#include "CoreMinimal.h"
#include "DataForgeTypes.h"

class IConsoleObject;

struct FDataForgeGoogleRuleSetRequest
{
	FString GoogleParserPath;
	FString RuleSetPath;
	FString RowStructPath;
	FString OutputDataTablePath;
	FName PrimaryKey = NAME_None;
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

	IConsoleObject* Register();
	void Unregister(IConsoleObject*& Command);
}
