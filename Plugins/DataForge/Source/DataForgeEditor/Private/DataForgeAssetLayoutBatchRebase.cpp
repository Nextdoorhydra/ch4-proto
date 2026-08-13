#include "DataForgeAssetLayoutBatchRebase.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DataForgeAssetLayoutProfile.h"
#include "DataForgeAutoReconciler.h"
#include "DataForgeDependencyGraph.h"
#include "DataForgeEditorService.h"
#include "DataForgeRuleSet.h"
#include "Modules/ModuleManager.h"

namespace
{
	void AddBatchError(TArray<FDataForgeDiagnostic>& Diagnostics, const TCHAR* Code, const FString& Message)
	{
		FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = EDataForgeSeverity::Error;
		Diagnostic.Code = Code;
		Diagnostic.Message = Message;
	}

	bool HasBatchErrors(const TArray<FDataForgeDiagnostic>& Diagnostics)
	{
		return Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
		{
			return Diagnostic.Severity == EDataForgeSeverity::Error;
		});
	}

	UDataForgeRuleSet* MakeCandidateDraft(const FDataForgeAssetLayoutBatchEntry& Entry)
	{
		UDataForgeRuleSet* Source = Entry.RuleSet.Get();
		if (!Source) return nullptr;
		UDataForgeRuleSet* Draft = DuplicateObject<UDataForgeRuleSet>(Source, GetTransientPackage());
		Draft->SetFlags(RF_Transient);
		Draft->AssetRules = Entry.Candidate.AssetRules;
		Draft->GeneratedOutputs = Entry.Candidate.GeneratedOutputs;
		Draft->Bindings = Entry.Candidate.Bindings;
		Draft->ProfileOrigin = Entry.Candidate.ProfileOrigin;
		return Draft;
	}

	bool RefreshContentPreviews(FDataForgeAssetLayoutBatchPlan& Plan, bool bRequireSameRevisions)
	{
		bool bSuccess = true;
		for (FDataForgeAssetLayoutBatchEntry& Entry : Plan.Entries)
		{
			UDataForgeRuleSet* Draft = MakeCandidateDraft(Entry);
			if (!Draft)
			{
				AddBatchError(Plan.Diagnostics, TEXT("DF1631"), TEXT("A dependent RuleSet is no longer available."));
				bSuccess = false;
				continue;
			}
			FDataForgeApplyPlan FreshPlan;
			const FDataForgeResult PreviewResult = FDataForgeEditorService::Preview(*Draft, &FreshPlan);
			Plan.Diagnostics.Append(PreviewResult.Diagnostics);
			if (!PreviewResult.bSuccess)
			{
				AddBatchError(Plan.Diagnostics, TEXT("DF1632"), FString::Printf(TEXT("Candidate content Preview failed for %s."), *Entry.RuleSet->GetPathName()));
				bSuccess = false;
				continue;
			}
			if (bRequireSameRevisions && (FreshPlan.SourceRevision != Entry.ContentPlan.SourceRevision || FreshPlan.TargetRevision != Entry.ContentPlan.TargetRevision))
			{
				AddBatchError(Plan.Diagnostics, TEXT("DF1633"), FString::Printf(TEXT("Source or target content changed after Batch Preview for %s."), *Entry.RuleSet->GetPathName()));
				bSuccess = false;
				continue;
			}
			if (!bRequireSameRevisions) Entry.ContentPlan = MoveTemp(FreshPlan);
		}
		return bSuccess;
	}
}

FString FDataForgeAssetLayoutBatchPlan::MakeSummary() const
{
	int32 Moves = 0, Creates = 0, Updates = 0;
	for (const FDataForgeAssetLayoutBatchEntry& Entry : Entries)
	{
		Moves += Entry.ContentPlan.AssetMoveCount;
		Creates += Entry.ContentPlan.AssetCreateCount;
		Updates += Entry.ContentPlan.AssetUpdateCount;
	}
	return FString::Printf(TEXT("Profile Batch Rebase | RuleSets: %d | Managed Creates: %d | Moves: %d | Updates: %d | Diagnostics: %d | %s"),
		Entries.Num(), Creates, Moves, Updates, Diagnostics.Num(), bSuccess ? TEXT("Ready") : TEXT("Blocked"));
}

TArray<UDataForgeRuleSet*> FDataForgeAssetLayoutBatchRebase::FindDependents(const UDataForgeAssetLayoutProfile& Profile)
{
	TArray<FAssetData> Assets;
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get()
		.GetAssetsByClass(UDataForgeRuleSet::StaticClass()->GetClassPathName(), Assets, true);
	TArray<UDataForgeRuleSet*> Result;
	for (const FAssetData& Asset : Assets)
	{
		UDataForgeRuleSet* RuleSet = Cast<UDataForgeRuleSet>(Asset.GetAsset());
		if (RuleSet && RuleSet->ProfileOrigin.ProfileId == Profile.ProfileId) Result.Add(RuleSet);
	}
	Result.Sort([](const UDataForgeRuleSet& Left, const UDataForgeRuleSet& Right) { return Left.GetPathName() < Right.GetPathName(); });
	return Result;
}

FDataForgeAssetLayoutBatchPlan FDataForgeAssetLayoutBatchRebase::Preview(UDataForgeAssetLayoutProfile& Profile)
{
	const TArray<UDataForgeRuleSet*> Dependents = FindDependents(Profile);
	return Preview(Profile, Dependents);
}

FDataForgeAssetLayoutBatchPlan FDataForgeAssetLayoutBatchRebase::Preview(
	UDataForgeAssetLayoutProfile& Profile,
	TConstArrayView<UDataForgeRuleSet*> RuleSets)
{
	FDataForgeAssetLayoutBatchPlan Plan;
	Plan.Profile = &Profile;
	Plan.ProfileRevisionHash = FDataForgeAssetLayoutMaterializer::ComputeLayoutRevisionHash(Profile);
	TSet<UDataForgeRuleSet*> Seen;
	for (UDataForgeRuleSet* RuleSet : RuleSets)
	{
		if (!RuleSet || Seen.Contains(RuleSet)) continue;
		Seen.Add(RuleSet);
		if (RuleSet->ProfileOrigin.ProfileId != Profile.ProfileId)
		{
			AddBatchError(Plan.Diagnostics, TEXT("DF1634"), FString::Printf(TEXT("RuleSet %s does not use the selected Profile."), *RuleSet->GetPathName()));
			continue;
		}

		UDataForgeRuleSet* ProbeDraft = DuplicateObject<UDataForgeRuleSet>(RuleSet, GetTransientPackage());
		ProbeDraft->SetFlags(RF_Transient);
		FDataForgeDataSet DataSet;
		const FDataForgeResult Probe = FDataForgeEditorService::Probe(*ProbeDraft, &DataSet);
		Plan.Diagnostics.Append(Probe.Diagnostics);
		if (!Probe.bSuccess)
		{
			AddBatchError(Plan.Diagnostics, TEXT("DF1635"), FString::Printf(TEXT("Source Probe failed for %s."), *RuleSet->GetPathName()));
			continue;
		}

		FDataForgeAssetLayoutBatchEntry& Entry = Plan.Entries.AddDefaulted_GetRef();
		Entry.RuleSet = RuleSet;
		Entry.Candidate = FDataForgeAssetLayoutAuthoring::PreviewRebase(
			*RuleSet, Profile, RuleSet->ProfileOrigin.ParameterValues, DataSet.Columns);
		Plan.Diagnostics.Append(Entry.Candidate.Diagnostics);
		if (!Entry.Candidate.bSuccess) continue;
	}

	const bool bContentPreviewSucceeded = RefreshContentPreviews(Plan, false);
	ValidateGlobalCollisions(Plan);
	Plan.bSuccess = !Plan.Entries.IsEmpty() && bContentPreviewSucceeded && !HasBatchErrors(Plan.Diagnostics)
		&& Plan.Entries.Num() == Seen.Num();
	return Plan;
}

void FDataForgeAssetLayoutBatchRebase::ValidateGlobalCollisions(FDataForgeAssetLayoutBatchPlan& Plan)
{
	TMap<FString, FString> OwnerByPackage;
	auto AddPackage = [&Plan, &OwnerByPackage](const FString& PackageName, const FString& Owner)
	{
		if (PackageName.IsEmpty()) return;
		if (const FString* ExistingOwner = OwnerByPackage.Find(PackageName); ExistingOwner && *ExistingOwner != Owner)
		{
			AddBatchError(Plan.Diagnostics, TEXT("DF1630"), FString::Printf(TEXT("Cross-RuleSet path collision at %s between %s and %s."), *PackageName, **ExistingOwner, *Owner));
			return;
		}
		OwnerByPackage.Add(PackageName, Owner);
	};
	for (const FDataForgeAssetLayoutBatchEntry& Entry : Plan.Entries)
	{
		const UDataForgeRuleSet* RuleSet = Entry.RuleSet.Get();
		if (!RuleSet) continue;
		const FString RuleSetPath = RuleSet->GetPathName();
		AddPackage(RuleSet->Output.AssetPath, RuleSetPath + TEXT(" [DataTable]"));
		for (const FDataForgePlannedAsset& Asset : Entry.ContentPlan.ManagedAssets)
		{
			AddPackage(Asset.PackageName, RuleSetPath + TEXT(" [") + Asset.OutputName.ToString() + TEXT("]"));
		}
	}
}

FDataForgeResult FDataForgeAssetLayoutBatchRebase::Apply(FDataForgeAssetLayoutBatchPlan& Plan)
{
	FDataForgeResult Result;
	UDataForgeAssetLayoutProfile* Profile = Plan.Profile.Get();
	if (!Plan.bSuccess || !Profile)
	{
		Result.Diagnostics = Plan.Diagnostics;
		Result.Summary = TEXT("Batch Rebase has no successful Preview to apply.");
		return Result;
	}
	if (Plan.ProfileRevisionHash != FDataForgeAssetLayoutMaterializer::ComputeLayoutRevisionHash(*Profile))
	{
		AddBatchError(Result.Diagnostics, TEXT("DF1636"), TEXT("The Profile changed after Batch Preview."));
		Result.Summary = TEXT("Batch Rebase is stale. Preview again.");
		return Result;
	}
	for (const FDataForgeAssetLayoutBatchEntry& Entry : Plan.Entries)
	{
		if (!Entry.RuleSet.IsValid() || !FDataForgeAssetLayoutAuthoring::IsRebaseCandidateCurrent(*Entry.RuleSet.Get(), Entry.Candidate))
		{
			AddBatchError(Result.Diagnostics, TEXT("DF1637"), TEXT("A dependent RuleSet changed after Batch Preview."));
			Result.Summary = TEXT("Batch Rebase is stale. Nothing was changed.");
			return Result;
		}
	}
	FDataForgeAssetLayoutBatchPlan Revalidated = Plan;
	Revalidated.Diagnostics.Reset();
	if (!RefreshContentPreviews(Revalidated, true))
	{
		Result.Diagnostics = Revalidated.Diagnostics;
		Result.Summary = TEXT("Batch Rebase content changed after Preview. Nothing was changed.");
		return Result;
	}

	TArray<const UDataForgeRuleSet*> Roots;
	for (const FDataForgeAssetLayoutBatchEntry& Entry : Plan.Entries) Roots.Add(Entry.RuleSet.Get());
	TArray<const UDataForgeRuleSet*> FullOrder;
	if (!FDataForgeDependencyGraph::BuildExecutionOrder(Roots, FullOrder, Result.Diagnostics))
	{
		Result.Summary = TEXT("Batch Rebase dependency graph is invalid. Nothing was changed.");
		return Result;
	}
	TMap<UDataForgeRuleSet*, const FDataForgeAssetLayoutBatchEntry*> EntryByRuleSet;
	for (const FDataForgeAssetLayoutBatchEntry& Entry : Plan.Entries) EntryByRuleSet.Add(Entry.RuleSet.Get(), &Entry);
	for (const UDataForgeRuleSet* Node : FullOrder)
	{
		UDataForgeRuleSet* RuleSet = const_cast<UDataForgeRuleSet*>(Node);
		const FDataForgeAssetLayoutBatchEntry* const* Entry = EntryByRuleSet.Find(RuleSet);
		if (!Entry) continue;
		const FDataForgeResult Rebase = FDataForgeAssetLayoutAuthoring::ApplyRebase(*RuleSet, (*Entry)->Candidate);
		Result.Diagnostics.Append(Rebase.Diagnostics);
		if (!Rebase.bSuccess)
		{
			Result.Summary = FString::Printf(TEXT("Batch Rebase failed while persisting %s."), *RuleSet->GetPathName());
			return Result;
		}
		FDataForgeAutoReconciler::Get().Cancel(*RuleSet);
	}
	for (const UDataForgeRuleSet* Node : FullOrder)
	{
		UDataForgeRuleSet* RuleSet = const_cast<UDataForgeRuleSet*>(Node);
		if (!EntryByRuleSet.Contains(RuleSet)) continue;
		const FDataForgeResult Preview = FDataForgeEditorService::Preview(*RuleSet);
		Result.Diagnostics.Append(Preview.Diagnostics);
		if (!Preview.bSuccess)
		{
			Result.Summary = FString::Printf(TEXT("Batch content Preview failed after persisting %s."), *RuleSet->GetPathName());
			return Result;
		}
	}
	int32 Applied = 0;
	for (const UDataForgeRuleSet* Node : FullOrder)
	{
		UDataForgeRuleSet* RuleSet = const_cast<UDataForgeRuleSet*>(Node);
		if (!EntryByRuleSet.Contains(RuleSet)) continue;
		const FDataForgeResult ApplyResult = FDataForgeEditorService::Apply(*RuleSet);
		Result.Diagnostics.Append(ApplyResult.Diagnostics);
		if (!ApplyResult.bSuccess)
		{
			Result.Summary = FString::Printf(TEXT("Batch Apply failed at %s after %d RuleSet(s)."), *RuleSet->GetPathName(), Applied);
			return Result;
		}
		FDataForgeAutoReconciler::Get().Cancel(*RuleSet);
		++Applied;
	}
	Plan.bSuccess = false;
	Result.bSuccess = true;
	Result.Summary = FString::Printf(TEXT("Batch Rebase and Apply succeeded for %d RuleSet(s)."), Applied);
	return Result;
}
