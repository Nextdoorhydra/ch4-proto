#include "DataForgeCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DataForgeDependencyGraph.h"
#include "DataForgeEditorService.h"
#include "DataForgeRuleSet.h"
#include "DataForgeRuleSetSnapshot.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"

namespace DataForgeCommandlet
{
	void LogDiagnostics(const TArray<FDataForgeDiagnostic>& Diagnostics)
	{
		for (const FDataForgeDiagnostic& Diagnostic : Diagnostics)
		{
			const TCHAR* Verbosity = Diagnostic.Severity == EDataForgeSeverity::Error
				? TEXT("Error")
				: Diagnostic.Severity == EDataForgeSeverity::Warning ? TEXT("Warning") : TEXT("Info");
			UE_LOG(LogTemp, Display, TEXT("DataForge %s %s: %s"), Verbosity, *Diagnostic.Code, *Diagnostic.Message);
		}
	}

	bool LoadRoots(const FString& Params, TArray<const UDataForgeRuleSet*>& OutRoots)
	{
		if (FParse::Param(*Params, TEXT("All")))
		{
			IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
			AssetRegistry.SearchAllAssets(true);
			TArray<FAssetData> Assets;
			AssetRegistry.GetAssetsByClass(UDataForgeRuleSet::StaticClass()->GetClassPathName(), Assets, true);
			for (const FAssetData& Asset : Assets)
			{
				if (const UDataForgeRuleSet* RuleSet = Cast<UDataForgeRuleSet>(Asset.GetAsset()))
				{
					OutRoots.Add(RuleSet);
				}
			}
			if (OutRoots.IsEmpty())
			{
				UE_LOG(LogTemp, Error, TEXT("DataForge -All found no RuleSet assets."));
				return false;
			}
			return true;
		}

		FString RuleSetPath;
		if (!FParse::Value(*Params, TEXT("RuleSet="), RuleSetPath))
		{
			UE_LOG(LogTemp, Error, TEXT("DataForge requires exactly one of -RuleSet=/Game/Path/RS_Name or -All."));
			return false;
		}
		if (FPackageName::IsValidLongPackageName(RuleSetPath))
		{
			RuleSetPath += TEXT(".") + FPackageName::GetLongPackageAssetName(RuleSetPath);
		}

		const UDataForgeRuleSet* RuleSet = LoadObject<UDataForgeRuleSet>(nullptr, *RuleSetPath);
		if (!RuleSet)
		{
			UE_LOG(LogTemp, Error, TEXT("Could not load DataForge RuleSet: %s"), *RuleSetPath);
			return false;
		}
		OutRoots.Add(RuleSet);
		return true;
	}

	bool HasChanges(const FDataForgeApplyPlan& Plan)
	{
		return Plan.CreateCount > 0
			|| Plan.UpdateCount > 0
			|| Plan.OrphanCount > 0
			|| Plan.AssetCreateCount > 0
			|| Plan.AssetMoveCount > 0
			|| Plan.AssetUpdateCount > 0
			|| Plan.AssetOrphanCount > 0;
	}
}

UDataForgeCommandlet::UDataForgeCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UDataForgeCommandlet::Main(const FString& Params)
{
	const bool bAll = FParse::Param(*Params, TEXT("All"));
	FString ExplicitRuleSetPath;
	const bool bHasExplicitRuleSet = FParse::Value(*Params, TEXT("RuleSet="), ExplicitRuleSetPath);
	if (bAll == bHasExplicitRuleSet)
	{
		UE_LOG(LogTemp, Error, TEXT("DataForge requires exactly one of -RuleSet or -All."));
		return 2;
	}

	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	const bool bCleanupOrphans = FParse::Param(*Params, TEXT("CleanupOrphans"));
	const bool bExportSnapshots = FParse::Param(*Params, TEXT("ExportSnapshots"));
	const bool bVerifySnapshots = FParse::Param(*Params, TEXT("VerifySnapshots"));
	const bool bFailOnChanges = FParse::Param(*Params, TEXT("FailOnChanges"));
	const int32 ActionModeCount = static_cast<int32>(bApply)
		+ static_cast<int32>(bCleanupOrphans)
		+ static_cast<int32>(bExportSnapshots)
		+ static_cast<int32>(bVerifySnapshots);
	if (ActionModeCount > 1 || (bFailOnChanges && ActionModeCount > 0))
	{
		UE_LOG(LogTemp, Error, TEXT("DataForge action modes -Apply, -CleanupOrphans, -ExportSnapshots, and -VerifySnapshots are mutually exclusive; none can be combined with -FailOnChanges."));
		return 2;
	}

	TArray<const UDataForgeRuleSet*> Roots;
	if (!DataForgeCommandlet::LoadRoots(Params, Roots))
	{
		return 2;
	}

	TArray<const UDataForgeRuleSet*> ExecutionOrder;
	TArray<FDataForgeDiagnostic> GraphDiagnostics;
	if (!FDataForgeDependencyGraph::BuildExecutionOrder(Roots, ExecutionOrder, GraphDiagnostics))
	{
		DataForgeCommandlet::LogDiagnostics(GraphDiagnostics);
		return 1;
	}

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DataForge"));
	const FString PluginVersion = Plugin.IsValid() ? Plugin->GetDescriptor().VersionName : TEXT("unknown");
	UE_LOG(
		LogTemp,
		Display,
		TEXT("DataForge CI run: mode=%s rules=%d plugin=%s"),
		bApply ? TEXT("Apply")
			: bCleanupOrphans ? TEXT("CleanupOrphans")
			: bExportSnapshots ? TEXT("ExportSnapshots")
			: bVerifySnapshots ? TEXT("VerifySnapshots")
			: FParse::Param(*Params, TEXT("Preview")) ? TEXT("Preview") : TEXT("ValidateOnly"),
		ExecutionOrder.Num(),
		*PluginVersion);

	bool bUnexpectedChanges = false;
	for (const UDataForgeRuleSet* ConstRuleSet : ExecutionOrder)
	{
		UDataForgeRuleSet* RuleSet = const_cast<UDataForgeRuleSet*>(ConstRuleSet);
		FDataForgeApplyPlan Plan;
		const FDataForgeResult PreviewResult = FDataForgeEditorService::Preview(*RuleSet, &Plan);
		if (!PreviewResult.bSuccess)
		{
			return 1;
		}

		UE_LOG(
			LogTemp,
			Display,
			TEXT("DataForge CI result: rule=%s ruleVersion=%d sourceRevision=%s summary=%s"),
			*RuleSet->GetPathName(),
			RuleSet->RuleVersion,
			*Plan.SourceRevision,
			*Plan.MakeSummary());
		bUnexpectedChanges |= DataForgeCommandlet::HasChanges(Plan);

		if (bApply && !FDataForgeEditorService::Apply(*RuleSet).bSuccess)
		{
			return 1;
		}
		if (bCleanupOrphans && !FDataForgeEditorService::CleanupOrphans(*RuleSet).bSuccess)
		{
			return 1;
		}
		if (bExportSnapshots)
		{
			const FDataForgeResult SnapshotResult = FDataForgeRuleSetSnapshot::Export(*RuleSet);
			DataForgeCommandlet::LogDiagnostics(SnapshotResult.Diagnostics);
			UE_LOG(LogTemp, Display, TEXT("%s"), *SnapshotResult.Summary);
			if (!SnapshotResult.bSuccess)
			{
				return 1;
			}
		}
		if (bVerifySnapshots)
		{
			const FDataForgeResult SnapshotResult = FDataForgeRuleSetSnapshot::Verify(*RuleSet);
			DataForgeCommandlet::LogDiagnostics(SnapshotResult.Diagnostics);
			UE_LOG(LogTemp, Display, TEXT("%s"), *SnapshotResult.Summary);
			if (!SnapshotResult.bSuccess)
			{
				return 1;
			}
		}
	}

	if (bFailOnChanges && bUnexpectedChanges)
	{
		UE_LOG(LogTemp, Error, TEXT("DataForge validation detected materialization changes and -FailOnChanges was specified."));
		return 3;
	}
	return 0;
}
