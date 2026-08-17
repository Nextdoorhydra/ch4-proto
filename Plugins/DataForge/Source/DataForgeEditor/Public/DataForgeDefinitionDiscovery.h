#pragma once

#include "CoreMinimal.h"
#include "DataForgeAuthoringTypes.h"
#include "DataForgeTypes.h"

class UDataForgeAssetLayoutRecipe;
class UDataForgeFolderSourceConfig;
class UDataForgeNamingPolicy;

struct DATAFORGEEDITOR_API FDataForgeDefinitionDiscoveryResult
{
	FDataForgeAuthoringDecision Decision;
	TWeakObjectPtr<UDataForgeNamingPolicy> NamingPolicy;
	TWeakObjectPtr<UDataForgeAssetLayoutRecipe> LayoutRecipe;
	TWeakObjectPtr<UDataForgeFolderSourceConfig> FolderSource;
	TArray<FDataForgeDiagnostic> Diagnostics;

	bool IsResolved() const;
};

/** Read-only discovery. Exact-root Folder Source chains are preferred over project-wide policies. */
class DATAFORGEEDITOR_API FDataForgeDefinitionDiscovery
{
public:
	static FDataForgeDefinitionDiscoveryResult Discover(
		const FString& AssetSearchRoot,
		UDataForgeNamingPolicy* ExplicitNamingPolicy = nullptr);
};
