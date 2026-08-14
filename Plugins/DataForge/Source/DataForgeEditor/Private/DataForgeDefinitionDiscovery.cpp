#include "DataForgeDefinitionDiscovery.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DataForgeFolderSource.h"
#include "DataForgeNamingPolicy.h"
#include "Modules/ModuleManager.h"

namespace DataForgeDefinitionDiscovery
{
	FString NormalizeFolder(FString Folder)
	{
		Folder.TrimStartAndEndInline();
		Folder.ReplaceInline(TEXT("\\"), TEXT("/"));
		while (Folder.EndsWith(TEXT("/"))) Folder.LeftChopInline(1);
		return Folder;
	}

	void AddDiagnostic(TArray<FDataForgeDiagnostic>& Diagnostics, EDataForgeSeverity Severity, const TCHAR* Code, const FString& Message)
	{
		FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = Severity;
		Diagnostic.Code = Code;
		Diagnostic.Message = Message;
	}

	bool ResolveChain(
		UDataForgeFolderSourceConfig* FolderSource,
		UDataForgeAssetLayoutRecipe*& OutRecipe,
		UDataForgeNamingPolicy*& OutPolicy)
	{
		OutRecipe = FolderSource ? FolderSource->LayoutRecipe.LoadSynchronous() : nullptr;
		OutPolicy = OutRecipe ? OutRecipe->NamingPolicy.LoadSynchronous() : nullptr;
		return OutPolicy && FDataForgeNamingPolicyResolver::ValidatePolicy(*OutPolicy).bSuccess;
	}
}

bool FDataForgeDefinitionDiscoveryResult::IsResolved() const
{
	return NamingPolicy.IsValid()
		&& (Decision.Disposition == EDataForgeInferenceDisposition::Exact
			|| Decision.Disposition == EDataForgeInferenceDisposition::Recommended);
}

FDataForgeDefinitionDiscoveryResult FDataForgeDefinitionDiscovery::Discover(
	const FString& AssetSearchRoot,
	UDataForgeNamingPolicy* ExplicitNamingPolicy)
{
	using namespace DataForgeDefinitionDiscovery;
	FDataForgeDefinitionDiscoveryResult Result;
	Result.Decision.DecisionId = TEXT("ReusableDefinitions");
	Result.Decision.bRequired = true;
	const FString Root = NormalizeFolder(AssetSearchRoot);
	if (ExplicitNamingPolicy)
	{
		const FDataForgeResult Validation = FDataForgeNamingPolicyResolver::ValidatePolicy(*ExplicitNamingPolicy);
		if (!Validation.bSuccess)
		{
			Result.Decision.Disposition = EDataForgeInferenceDisposition::Conflict;
			Result.Diagnostics = Validation.Diagnostics;
			return Result;
		}
		Result.NamingPolicy = ExplicitNamingPolicy;
		Result.Decision.SelectedValue = ExplicitNamingPolicy->GetPathName();
		Result.Decision.Disposition = EDataForgeInferenceDisposition::Exact;
	}

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> FolderAssets;
	Registry.GetAssetsByClass(UDataForgeFolderSourceConfig::StaticClass()->GetClassPathName(), FolderAssets, true);
	FolderAssets.Sort([](const FAssetData& A, const FAssetData& B)
	{
		return A.GetSoftObjectPath().ToString() < B.GetSoftObjectPath().ToString();
	});
	struct FChain
	{
		UDataForgeFolderSourceConfig* Folder = nullptr;
		UDataForgeAssetLayoutRecipe* Recipe = nullptr;
		UDataForgeNamingPolicy* Policy = nullptr;
	};
	TArray<FChain> ExactChains;
	for (const FAssetData& Asset : FolderAssets)
	{
		UDataForgeFolderSourceConfig* Folder = Cast<UDataForgeFolderSourceConfig>(Asset.GetAsset());
		if (!Folder || NormalizeFolder(Folder->RootFolder) != Root) continue;
		FChain& Chain = ExactChains.AddDefaulted_GetRef();
		Chain.Folder = Folder;
		if (!ResolveChain(Folder, Chain.Recipe, Chain.Policy)) ExactChains.Pop();
	}
	if (ExplicitNamingPolicy)
	{
		TArray<const FChain*> Compatible;
		for (const FChain& Chain : ExactChains)
		{
			if (Chain.Policy == ExplicitNamingPolicy) Compatible.Add(&Chain);
		}
		if (Compatible.Num() == 1)
		{
			Result.FolderSource = Compatible[0]->Folder;
			Result.LayoutRecipe = Compatible[0]->Recipe;
		}
		return Result;
	}

	for (const FChain& Chain : ExactChains) Result.Decision.Alternatives.Add(Chain.Folder->GetPathName());
	if (ExactChains.Num() == 1)
	{
		Result.FolderSource = ExactChains[0].Folder;
		Result.LayoutRecipe = ExactChains[0].Recipe;
		Result.NamingPolicy = ExactChains[0].Policy;
		Result.Decision.SelectedValue = ExactChains[0].Folder->GetPathName();
		Result.Decision.Disposition = EDataForgeInferenceDisposition::Exact;
		return Result;
	}
	if (ExactChains.Num() > 1)
	{
		Result.Decision.Disposition = EDataForgeInferenceDisposition::Ambiguous;
		AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Warning, TEXT("DF2101"),
			FString::Printf(TEXT("Asset root '%s' has multiple exact Folder Source Configs. Select a Naming Policy or FSC explicitly."), *Root));
		return Result;
	}

	TArray<FAssetData> PolicyAssets;
	Registry.GetAssetsByClass(UDataForgeNamingPolicy::StaticClass()->GetClassPathName(), PolicyAssets, true);
	PolicyAssets.Sort([](const FAssetData& A, const FAssetData& B)
	{
		return A.GetSoftObjectPath().ToString() < B.GetSoftObjectPath().ToString();
	});
	TArray<UDataForgeNamingPolicy*> ValidPolicies;
	for (const FAssetData& Asset : PolicyAssets)
	{
		UDataForgeNamingPolicy* Policy = Cast<UDataForgeNamingPolicy>(Asset.GetAsset());
		if (Policy && FDataForgeNamingPolicyResolver::ValidatePolicy(*Policy).bSuccess)
		{
			ValidPolicies.Add(Policy);
			Result.Decision.Alternatives.Add(Policy->GetPathName());
		}
	}
	if (ValidPolicies.Num() == 1)
	{
		Result.NamingPolicy = ValidPolicies[0];
		Result.Decision.SelectedValue = ValidPolicies[0]->GetPathName();
		Result.Decision.Disposition = EDataForgeInferenceDisposition::Recommended;
	}
	else if (ValidPolicies.Num() > 1)
	{
		Result.Decision.Disposition = EDataForgeInferenceDisposition::Ambiguous;
		AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Warning, TEXT("DF2102"),
			TEXT("Multiple valid Naming Policies exist and no exact Folder Source Config matches the selected root."));
	}
	else
	{
		Result.Decision.Disposition = EDataForgeInferenceDisposition::Unsupported;
		AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF2103"),
			TEXT("No valid reusable Naming Policy is available."));
	}
	return Result;
}
