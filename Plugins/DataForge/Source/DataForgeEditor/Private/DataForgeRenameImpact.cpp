#include "DataForgeRenameImpact.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DataForgePipeline.h"
#include "DataForgeFolderSource.h"
#include "DataForgeRuleSet.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"

namespace DataForgeRenameImpact
{
	bool ContainsPath(const FString& ExportedValue, const FString& ObjectPath)
	{
		return !ObjectPath.IsEmpty() && ExportedValue.Contains(ObjectPath, ESearchCase::CaseSensitive);
	}

	bool IsInside(FString PackageName, FString Root)
	{
		Root.ReplaceInline(TEXT("\\"), TEXT("/"));
		while (Root.EndsWith(TEXT("/"))) Root.LeftChopInline(1);
		return !Root.IsEmpty() && (PackageName == Root || PackageName.StartsWith(Root + TEXT("/")));
	}

	bool MayAffect(const UDataForgeRuleSet& RuleSet, const FSoftObjectPath& OldObjectPath, const FSoftObjectPath& NewObjectPath)
	{
		const FString OldPackage = FPackageName::ObjectPathToPackageName(OldObjectPath.ToString());
		const FString NewPackage = FPackageName::ObjectPathToPackageName(NewObjectPath.ToString());
		for (const FDataForgeAssociationSourceRule& Association : RuleSet.AssociationSources)
		{
			if (Association.Source.AdapterId != TEXT("AssetRegistryFolder")) continue;
			const UDataForgeFolderSourceConfig* Config = Cast<UDataForgeFolderSourceConfig>(Association.Source.SourceAsset.LoadSynchronous());
			if (Config && (IsInside(OldPackage, Config->RootFolder) || IsInside(NewPackage, Config->RootFolder))) return true;
		}
		for (const FDataForgeAssetRule& Rule : RuleSet.AssetRules)
		{
			if (IsInside(OldPackage, Rule.BaseFolder) || IsInside(NewPackage, Rule.BaseFolder)) return true;
		}
		return false;
	}
}

FString FDataForgeRenameImpactEntry::MakeSummary() const
{
	const UDataForgeRuleSet* SourceRuleSet = RuleSet.Get();
	return FString::Printf(TEXT("%s | %s | %d rebind(s) | %d diagnostic(s)"),
		SourceRuleSet ? *SourceRuleSet->GetPathName() : TEXT("<unavailable RuleSet>"),
		bSuccess ? TEXT("Ready") : TEXT("Blocked"), Rebinds.Num(), Diagnostics.Num());
}

bool FDataForgeRenameImpact::IsSafe() const
{
	return Entries.ContainsByPredicate([](const FDataForgeRenameImpactEntry& Entry) { return Entry.bSuccess; })
		&& !Entries.ContainsByPredicate([](const FDataForgeRenameImpactEntry& Entry) { return !Entry.bSuccess; });
}

FString FDataForgeRenameImpact::MakeSummary() const
{
	int32 Ready = 0;
	int32 Blocked = 0;
	int32 Rebinds = 0;
	for (const FDataForgeRenameImpactEntry& Entry : Entries)
	{
		Entry.bSuccess ? ++Ready : ++Blocked;
		Rebinds += Entry.Rebinds.Num();
	}
	return FString::Printf(TEXT("Rename Impact | Ready: %d | Blocked: %d | Rebinds: %d"), Ready, Blocked, Rebinds);
}

FDataForgeRenameImpactEntry FDataForgeRenameImpactAnalyzer::AnalyzeRuleSet(
	const UDataForgeRuleSet& RuleSet,
	const FSoftObjectPath& OldObjectPath,
	const FSoftObjectPath& NewObjectPath)
{
	FDataForgeRenameImpactEntry Impact;
	Impact.RuleSet = &RuleSet;
	FCompiledDataForgeRuleSet Compiled;
	if (!FDataForgeCompiler::Compile(RuleSet, Compiled, Impact.Diagnostics)) return Impact;
	const FDataForgeApplyPlan Plan = FDataForgeCompiler::BuildPlan(Compiled);
	Impact.Diagnostics.Append(Plan.Diagnostics);
	Impact.bSuccess = !Plan.HasErrors();
	if (!Impact.bSuccess) return Impact;

	const FString OldPath = OldObjectPath.ToString();
	const FString NewPath = NewObjectPath.ToString();
	for (const FDataForgePlannedAsset& Asset : Plan.ManagedAssets)
	{
		for (const FDataForgePlannedPropertyWrite& Write : Asset.PropertyWrites)
		{
			if (!DataForgeRenameImpact::ContainsPath(Write.PreviousValue, OldPath)
				|| !DataForgeRenameImpact::ContainsPath(Write.ExportedValue, NewPath)) continue;
			FDataForgeRenameRebind& Rebind = Impact.Rebinds.AddDefaulted_GetRef();
			Rebind.RecordId = Asset.RecordId;
			Rebind.OutputName = Asset.OutputName;
			Rebind.PropertyPath = Write.PropertyPath;
			Rebind.OldObjectPath = OldPath;
			Rebind.NewObjectPath = NewPath;
		}
	}
	return Impact;
}

FDataForgeRenameImpact FDataForgeRenameImpactAnalyzer::AnalyzeProject(
	const FSoftObjectPath& OldObjectPath,
	const FSoftObjectPath& NewObjectPath)
{
	FDataForgeRenameImpact Impact;
	TArray<FAssetData> RuleSetAssets;
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get()
		.GetAssetsByClass(UDataForgeRuleSet::StaticClass()->GetClassPathName(), RuleSetAssets, true);
	RuleSetAssets.Sort([](const FAssetData& Left, const FAssetData& Right) { return Left.GetSoftObjectPath().ToString() < Right.GetSoftObjectPath().ToString(); });
	for (const FAssetData& AssetData : RuleSetAssets)
	{
		const UDataForgeRuleSet* RuleSet = Cast<UDataForgeRuleSet>(AssetData.GetAsset());
		if (!RuleSet || !DataForgeRenameImpact::MayAffect(*RuleSet, OldObjectPath, NewObjectPath)) continue;
		FDataForgeRenameImpactEntry Entry = AnalyzeRuleSet(*RuleSet, OldObjectPath, NewObjectPath);
		if (!Entry.Rebinds.IsEmpty() || !Entry.bSuccess) Impact.Entries.Add(MoveTemp(Entry));
	}
	return Impact;
}
