#include "DataForgeAssetLayoutAuthoring.h"

#include "DataForgeAssetLayoutProfile.h"
#include "DataForgeRuleSet.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "DataForgeAssetLayoutAuthoring"

namespace
{
	bool RulesEqual(const FDataForgeAssetRule& A, const FDataForgeAssetRule& B)
	{
		return A.RuleId == B.RuleId
			&& A.Ownership == B.Ownership
			&& A.BaseFolder == B.BaseFolder
			&& A.SubfolderPattern == B.SubfolderPattern
			&& A.AssetNamePattern == B.AssetNamePattern;
	}

	int32 CountChangedFields(const FDataForgeAssetRule& Current, const FDataForgeAssetRule& Baseline)
	{
		return (Current.RuleId != Baseline.RuleId ? 1 : 0)
			+ (Current.Ownership != Baseline.Ownership ? 1 : 0)
			+ (Current.BaseFolder != Baseline.BaseFolder ? 1 : 0)
			+ (Current.SubfolderPattern != Baseline.SubfolderPattern ? 1 : 0)
			+ (Current.AssetNamePattern != Baseline.AssetNamePattern ? 1 : 0);
	}

	FDataForgeAssetRule MergeRule(
		const FDataForgeAssetRule& OldBaseline,
		const FDataForgeAssetRule& Current,
		const FDataForgeAssetRule& NewBaseline)
	{
		FDataForgeAssetRule Merged = NewBaseline;
		if (Current.RuleId != OldBaseline.RuleId) Merged.RuleId = Current.RuleId;
		if (Current.Ownership != OldBaseline.Ownership) Merged.Ownership = Current.Ownership;
		if (Current.BaseFolder != OldBaseline.BaseFolder) Merged.BaseFolder = Current.BaseFolder;
		if (Current.SubfolderPattern != OldBaseline.SubfolderPattern) Merged.SubfolderPattern = Current.SubfolderPattern;
		if (Current.AssetNamePattern != OldBaseline.AssetNamePattern) Merged.AssetNamePattern = Current.AssetNamePattern;
		return Merged;
	}

	FDataForgeResult Failure(const TCHAR* Code, const FString& Message)
	{
		FDataForgeResult Result;
		FDataForgeDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = EDataForgeSeverity::Error;
		Diagnostic.Code = Code;
		Diagnostic.Message = Message;
		Result.Summary = Message;
		return Result;
	}

	bool HasDuplicateRuleIds(const TArray<FDataForgeAssetRule>& Rules, FName& OutDuplicate)
	{
		TSet<FName> RuleIds;
		for (const FDataForgeAssetRule& Rule : Rules)
		{
			if (Rule.RuleId.IsNone() || RuleIds.Contains(Rule.RuleId))
			{
				OutDuplicate = Rule.RuleId;
				return true;
			}
			RuleIds.Add(Rule.RuleId);
		}
		return false;
	}

	void AppendStateField(FString& State, const FString& Value)
	{
		State += FString::FromInt(Value.Len());
		State += TEXT(":");
		State += Value;
		State += TEXT("|");
	}

	FString ComputeRuleStateHash(const UDataForgeRuleSet& RuleSet)
	{
		FString State;
		for (const FDataForgeAssetRule& Rule : RuleSet.AssetRules)
		{
			AppendStateField(State, Rule.RuleId.ToString());
			AppendStateField(State, FString::FromInt(static_cast<uint8>(Rule.Ownership)));
			AppendStateField(State, Rule.BaseFolder);
			AppendStateField(State, Rule.SubfolderPattern);
			AppendStateField(State, Rule.AssetNamePattern);
		}
		for (const FDataForgeGeneratedAssetOutputRule& Output : RuleSet.GeneratedOutputs)
		{
			AppendStateField(State, Output.OutputName.ToString());
			AppendStateField(State, FString::FromInt(static_cast<uint8>(Output.Type)));
			AppendStateField(State, GetPathNameSafe(Output.AssetClass.Get()));
			AppendStateField(State, Output.AssetRuleId.ToString());
		}
		for (const FDataForgeBindingRule& Binding : RuleSet.Bindings)
		{
			AppendStateField(State, FString::FromInt(static_cast<uint8>(Binding.Source)));
			AppendStateField(State, Binding.SourceColumn.ToString());
			AppendStateField(State, Binding.SourceOutput.ToString());
			AppendStateField(State, FString::FromInt(static_cast<uint8>(Binding.Target)));
			AppendStateField(State, Binding.TargetOutput.ToString());
			AppendStateField(State, Binding.TargetProperty);
			AppendStateField(State, Binding.AssetRuleId.ToString());
			AppendStateField(State, Binding.bRequired ? TEXT("1") : TEXT("0"));
		}
		AppendStateField(State, RuleSet.ProfileOrigin.ProfileId.ToString());
		AppendStateField(State, FString::FromInt(RuleSet.ProfileOrigin.MaterializedVersion));
		AppendStateField(State, RuleSet.ProfileOrigin.MaterializedHash);
		TArray<FName> ParameterNames;
		RuleSet.ProfileOrigin.ParameterValues.GetKeys(ParameterNames);
		ParameterNames.Sort(FNameLexicalLess());
		for (const FName Name : ParameterNames)
		{
			AppendStateField(State, Name.ToString());
			AppendStateField(State, RuleSet.ProfileOrigin.ParameterValues.FindRef(Name));
		}
		for (const FDataForgeMaterializedRuleOrigin& Origin : RuleSet.ProfileOrigin.Rules)
		{
			AppendStateField(State, Origin.GroupTemplateId.ToString());
			AppendStateField(State, Origin.RuleTemplateId.ToString());
			AppendStateField(State, Origin.BaselineRule.RuleId.ToString());
			AppendStateField(State, Origin.BaselineRule.BaseFolder);
			AppendStateField(State, Origin.BaselineRule.SubfolderPattern);
			AppendStateField(State, Origin.BaselineRule.AssetNamePattern);
		}
		FTCHARToUTF8 Utf8(*State);
		uint8 Hash[FSHA1::DigestSize];
		FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Hash);
		return BytesToHex(Hash, UE_ARRAY_COUNT(Hash));
	}

	bool IsRuleReferenced(const UDataForgeRuleSet& RuleSet, const FName RuleId)
	{
		return RuleSet.GeneratedOutputs.ContainsByPredicate([RuleId](const FDataForgeGeneratedAssetOutputRule& Output) { return Output.AssetRuleId == RuleId; })
			|| RuleSet.Bindings.ContainsByPredicate([RuleId](const FDataForgeBindingRule& Binding) { return Binding.AssetRuleId == RuleId; });
	}

	const FDataForgeAssetRule* FindCurrentRuleForOrigin(
		const UDataForgeRuleSet& RuleSet,
		const FDataForgeMaterializedRuleOrigin& Origin,
		const TSet<FName>& ConsumedRuleIds)
	{
		if (const FDataForgeAssetRule* ById = RuleSet.AssetRules.FindByPredicate([&](const FDataForgeAssetRule& Rule)
		{
			return Rule.RuleId == Origin.BaselineRule.RuleId && !ConsumedRuleIds.Contains(Rule.RuleId);
		})) return ById;

		// A local RuleId override removes the only direct key on the concrete rule. Accept a unique
		// candidate whose remaining layout fields still match the typed baseline.
		const FDataForgeAssetRule* Unique = nullptr;
		for (const FDataForgeAssetRule& Rule : RuleSet.AssetRules)
		{
			if (ConsumedRuleIds.Contains(Rule.RuleId)
				|| Rule.Ownership != Origin.BaselineRule.Ownership
				|| Rule.BaseFolder != Origin.BaselineRule.BaseFolder
				|| Rule.SubfolderPattern != Origin.BaselineRule.SubfolderPattern
				|| Rule.AssetNamePattern != Origin.BaselineRule.AssetNamePattern) continue;
			if (Unique) return nullptr;
			Unique = &Rule;
		}
		return Unique;
	}

	void AddCandidateError(FDataForgeAssetLayoutRebaseCandidate& Candidate, const TCHAR* Code, const FString& Message)
	{
		FDataForgeDiagnostic& Diagnostic = Candidate.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = EDataForgeSeverity::Error;
		Diagnostic.Code = Code;
		Diagnostic.Message = Message;
	}
}

FString FDataForgeAssetLayoutRebaseCandidate::MakeSummary() const
{
	int32 Added = 0, Updated = 0, Removed = 0, Preserved = 0, Errors = 0;
	for (const FDataForgeAssetLayoutRebaseRuleChange& Change : Changes)
	{
		switch (Change.Change)
		{
		case EDataForgeAssetLayoutRebaseChange::Added: ++Added; break;
		case EDataForgeAssetLayoutRebaseChange::Removed: ++Removed; break;
		case EDataForgeAssetLayoutRebaseChange::CustomPreserved: ++Preserved; break;
		default: ++Updated; break;
		}
	}
	for (const FDataForgeDiagnostic& Diagnostic : Diagnostics)
	{
		if (Diagnostic.Severity == EDataForgeSeverity::Error) ++Errors;
	}
	return FString::Printf(TEXT("Rebase Candidate | Added: %d | Updated: %d | Removed: %d | Custom Preserved: %d | Errors: %d"),
		Added, Updated, Removed, Preserved, Errors);
}

FString FDataForgeAssetLayoutAnalysis::MakeSummary() const
{
	const TCHAR* StatusText = TEXT("Manual");
	switch (Status)
	{
	case EDataForgeAssetLayoutStatus::InSync: StatusText = TEXT("In Sync"); break;
	case EDataForgeAssetLayoutStatus::Outdated: StatusText = TEXT("Outdated"); break;
	case EDataForgeAssetLayoutStatus::Modified: StatusText = TEXT("Modified"); break;
	case EDataForgeAssetLayoutStatus::MissingProfile: StatusText = TEXT("Missing Profile"); break;
	default: break;
	}
	return FString::Printf(
		TEXT("%s | Overrides: %d | Custom Rules: %d | Missing Profile Rules: %d"),
		StatusText,
		OverrideFieldCount,
		CustomRuleCount,
		MissingProfileRuleCount);
}

FDataForgeAssetLayoutAnalysis FDataForgeAssetLayoutAuthoring::Analyze(const UDataForgeRuleSet& RuleSet)
{
	FDataForgeAssetLayoutAnalysis Analysis;
	if (!RuleSet.ProfileOrigin.IsSet())
	{
		Analysis.CustomRuleCount = RuleSet.AssetRules.Num();
		for (const FDataForgeAssetRule& Rule : RuleSet.AssetRules)
		{
			FDataForgeAssetLayoutRuleAnalysis& RuleAnalysis = Analysis.Rules.AddDefaulted_GetRef();
			RuleAnalysis.RuleId = Rule.RuleId;
			RuleAnalysis.Status = EDataForgeAssetLayoutRuleStatus::Custom;
		}
		return Analysis;
	}

	UDataForgeAssetLayoutProfile* Profile = RuleSet.ProfileOrigin.Profile.LoadSynchronous();
	if (!Profile || Profile->ProfileId != RuleSet.ProfileOrigin.ProfileId)
	{
		Analysis.Status = EDataForgeAssetLayoutStatus::MissingProfile;
	}
	else
	{
		Analysis.CurrentProfileHash = FDataForgeAssetLayoutMaterializer::ComputeLayoutRevisionHash(*Profile);
		Analysis.Status = Analysis.CurrentProfileHash == RuleSet.ProfileOrigin.MaterializedHash
			? EDataForgeAssetLayoutStatus::InSync
			: EDataForgeAssetLayoutStatus::Outdated;
	}

	TSet<FName> ProfileRuleIds;
	for (const FDataForgeMaterializedRuleOrigin& Origin : RuleSet.ProfileOrigin.Rules)
	{
		ProfileRuleIds.Add(Origin.BaselineRule.RuleId);
		FDataForgeAssetLayoutRuleAnalysis& RuleAnalysis = Analysis.Rules.AddDefaulted_GetRef();
		RuleAnalysis.RuleId = Origin.BaselineRule.RuleId;
		const FDataForgeAssetRule* Current = RuleSet.AssetRules.FindByPredicate([&Origin](const FDataForgeAssetRule& Rule)
		{
			return Rule.RuleId == Origin.BaselineRule.RuleId;
		});
		if (!Current)
		{
			++Analysis.MissingProfileRuleCount;
			RuleAnalysis.Status = EDataForgeAssetLayoutRuleStatus::Missing;
			continue;
		}
		RuleAnalysis.OverrideFieldCount = CountChangedFields(*Current, Origin.BaselineRule);
		RuleAnalysis.Status = RuleAnalysis.OverrideFieldCount > 0
			? EDataForgeAssetLayoutRuleStatus::Override
			: EDataForgeAssetLayoutRuleStatus::Profile;
		Analysis.OverrideFieldCount += RuleAnalysis.OverrideFieldCount;
	}
	for (const FDataForgeAssetRule& Rule : RuleSet.AssetRules)
	{
		if (!ProfileRuleIds.Contains(Rule.RuleId))
		{
			++Analysis.CustomRuleCount;
			FDataForgeAssetLayoutRuleAnalysis& RuleAnalysis = Analysis.Rules.AddDefaulted_GetRef();
			RuleAnalysis.RuleId = Rule.RuleId;
			RuleAnalysis.Status = EDataForgeAssetLayoutRuleStatus::Custom;
		}
	}

	if (Analysis.Status == EDataForgeAssetLayoutStatus::InSync
		&& (Analysis.OverrideFieldCount > 0 || Analysis.CustomRuleCount > 0 || Analysis.MissingProfileRuleCount > 0))
	{
		Analysis.Status = EDataForgeAssetLayoutStatus::Modified;
	}
	return Analysis;
}

FDataForgeResult FDataForgeAssetLayoutAuthoring::Materialize(
	UDataForgeRuleSet& RuleSet,
	UDataForgeAssetLayoutProfile& Profile,
	const TMap<FName, FString>& ParameterValues,
	const TArray<FName>& SourceColumns)
{
	const FDataForgeAssetLayoutRebaseCandidate Candidate = PreviewRebase(RuleSet, Profile, ParameterValues, SourceColumns);
	return ApplyRebase(RuleSet, Candidate);
}

FDataForgeAssetLayoutRebaseCandidate FDataForgeAssetLayoutAuthoring::PreviewRebase(
	const UDataForgeRuleSet& RuleSet,
	UDataForgeAssetLayoutProfile& Profile,
	const TMap<FName, FString>& ParameterValues,
	const TArray<FName>& SourceColumns)
{
	FDataForgeAssetLayoutRebaseCandidate Candidate;
	Candidate.SourceStateHash = ComputeRuleStateHash(RuleSet);
	Candidate.GeneratedOutputs = RuleSet.GeneratedOutputs;
	Candidate.Bindings = RuleSet.Bindings;
	if (RuleSet.ProfileOrigin.IsSet() && RuleSet.ProfileOrigin.ProfileId != Profile.ProfileId)
	{
		AddCandidateError(Candidate, TEXT("DF1620"), TEXT("This RuleSet already uses a different Asset Layout Profile. Detach it before selecting another Profile."));
		return Candidate;
	}

	const FDataForgeAssetLayoutMaterialization Materialized = FDataForgeAssetLayoutMaterializer::Materialize(
		Profile,
		ParameterValues,
		&SourceColumns);
	if (!Materialized.bSuccess)
	{
		Candidate.Diagnostics = Materialized.Diagnostics;
		return Candidate;
	}
	Candidate.ProfileOrigin = Materialized.Origin;

	if (!RuleSet.ProfileOrigin.IsSet())
	{
		Candidate.AssetRules = Materialized.AssetRules;
		for (int32 Index = 0; Index < Materialized.AssetRules.Num(); ++Index)
		{
			FDataForgeAssetLayoutRebaseRuleChange& Change = Candidate.Changes.AddDefaulted_GetRef();
			Change.RuleTemplateId = Materialized.Origin.Rules[Index].RuleTemplateId;
			Change.CandidateRuleId = Materialized.AssetRules[Index].RuleId;
			Change.Change = EDataForgeAssetLayoutRebaseChange::Added;
		}
		for (const FDataForgeAssetRule& Custom : RuleSet.AssetRules)
		{
			Candidate.AssetRules.Add(Custom);
			FDataForgeAssetLayoutRebaseRuleChange& Change = Candidate.Changes.AddDefaulted_GetRef();
			Change.PreviousRuleId = Custom.RuleId;
			Change.CandidateRuleId = Custom.RuleId;
			Change.Change = EDataForgeAssetLayoutRebaseChange::CustomPreserved;
		}
	}
	else
	{
		TSet<FName> ConsumedCurrentRuleIds;
		for (int32 NewIndex = 0; NewIndex < Materialized.AssetRules.Num(); ++NewIndex)
		{
			const FDataForgeMaterializedRuleOrigin& NewOrigin = Materialized.Origin.Rules[NewIndex];
			const FDataForgeMaterializedRuleOrigin* OldOrigin = RuleSet.ProfileOrigin.Rules.FindByPredicate([&NewOrigin](const FDataForgeMaterializedRuleOrigin& Candidate)
			{
				return Candidate.RuleTemplateId == NewOrigin.RuleTemplateId;
			});
			const FDataForgeAssetRule* Current = OldOrigin ? FindCurrentRuleForOrigin(RuleSet, *OldOrigin, ConsumedCurrentRuleIds) : nullptr;
			if (OldOrigin && Current)
			{
				const FDataForgeAssetRule Merged = MergeRule(OldOrigin->BaselineRule, *Current, Materialized.AssetRules[NewIndex]);
				Candidate.AssetRules.Add(Merged);
				ConsumedCurrentRuleIds.Add(Current->RuleId);
				FDataForgeAssetLayoutRebaseRuleChange& Change = Candidate.Changes.AddDefaulted_GetRef();
				Change.RuleTemplateId = NewOrigin.RuleTemplateId;
				Change.PreviousRuleId = Current->RuleId;
				Change.CandidateRuleId = Merged.RuleId;
				Change.ChangedFieldCount = CountChangedFields(Merged, *Current);
				Change.Change = EDataForgeAssetLayoutRebaseChange::Updated;
				if (Current->RuleId == OldOrigin->BaselineRule.RuleId && Merged.RuleId != Current->RuleId)
				{
					for (FDataForgeGeneratedAssetOutputRule& Output : Candidate.GeneratedOutputs) if (Output.AssetRuleId == Current->RuleId) Output.AssetRuleId = Merged.RuleId;
					for (FDataForgeBindingRule& Binding : Candidate.Bindings) if (Binding.AssetRuleId == Current->RuleId) Binding.AssetRuleId = Merged.RuleId;
				}
			}
			else
			{
				Candidate.AssetRules.Add(Materialized.AssetRules[NewIndex]);
				FDataForgeAssetLayoutRebaseRuleChange& Change = Candidate.Changes.AddDefaulted_GetRef();
				Change.RuleTemplateId = NewOrigin.RuleTemplateId;
				Change.CandidateRuleId = Materialized.AssetRules[NewIndex].RuleId;
				Change.Change = EDataForgeAssetLayoutRebaseChange::Added;
			}
		}
		for (const FDataForgeMaterializedRuleOrigin& OldOrigin : RuleSet.ProfileOrigin.Rules)
		{
			if (Materialized.Origin.Rules.ContainsByPredicate([&OldOrigin](const FDataForgeMaterializedRuleOrigin& NewOrigin) { return NewOrigin.RuleTemplateId == OldOrigin.RuleTemplateId; })) continue;
			const FDataForgeAssetRule* Current = RuleSet.AssetRules.FindByPredicate([&OldOrigin](const FDataForgeAssetRule& Rule) { return Rule.RuleId == OldOrigin.BaselineRule.RuleId; });
			if (!Current) continue;
			ConsumedCurrentRuleIds.Add(Current->RuleId);
			if (!RulesEqual(*Current, OldOrigin.BaselineRule))
			{
				Candidate.AssetRules.Add(*Current);
				FDataForgeAssetLayoutRebaseRuleChange& Change = Candidate.Changes.AddDefaulted_GetRef();
				Change.RuleTemplateId = OldOrigin.RuleTemplateId;
				Change.PreviousRuleId = Current->RuleId;
				Change.CandidateRuleId = Current->RuleId;
				Change.Change = EDataForgeAssetLayoutRebaseChange::CustomPreserved;
			}
			else if (IsRuleReferenced(RuleSet, Current->RuleId))
			{
				AddCandidateError(Candidate, TEXT("DF1623"), FString::Printf(TEXT("Profile rule '%s' was removed but is still referenced by a Generated Output or Binding."), *Current->RuleId.ToString()));
			}
			else
			{
				FDataForgeAssetLayoutRebaseRuleChange& Change = Candidate.Changes.AddDefaulted_GetRef();
				Change.RuleTemplateId = OldOrigin.RuleTemplateId;
				Change.PreviousRuleId = Current->RuleId;
				Change.Change = EDataForgeAssetLayoutRebaseChange::Removed;
			}
		}
		for (const FDataForgeAssetRule& Current : RuleSet.AssetRules)
		{
			if (!ConsumedCurrentRuleIds.Contains(Current.RuleId))
			{
				Candidate.AssetRules.Add(Current);
				FDataForgeAssetLayoutRebaseRuleChange& Change = Candidate.Changes.AddDefaulted_GetRef();
				Change.PreviousRuleId = Current.RuleId;
				Change.CandidateRuleId = Current.RuleId;
				Change.Change = EDataForgeAssetLayoutRebaseChange::CustomPreserved;
			}
		}
	}

	FName DuplicateRuleId;
	if (HasDuplicateRuleIds(Candidate.AssetRules, DuplicateRuleId))
	{
		AddCandidateError(Candidate, TEXT("DF1621"), FString::Printf(TEXT("Profile materialization would create duplicate Asset Rule Id '%s'. Rename or remove the conflicting custom rule first."), *DuplicateRuleId.ToString()));
	}
	Candidate.bSuccess = !Candidate.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic) { return Diagnostic.Severity == EDataForgeSeverity::Error; });
	return Candidate;
}


FDataForgeResult FDataForgeAssetLayoutAuthoring::ApplyRebase(UDataForgeRuleSet& RuleSet, const FDataForgeAssetLayoutRebaseCandidate& Candidate)
{
	if (!Candidate.bSuccess)
	{
		FDataForgeResult Result;
		Result.Diagnostics = Candidate.Diagnostics;
		Result.Summary = TEXT("Asset Layout Profile validation failed. RuleSet was not changed.");
		return Result;
	}
	if (!IsRebaseCandidateCurrent(RuleSet, Candidate))
	{
		return Failure(TEXT("DF1622"), TEXT("The RuleSet changed after Rebase Preview. Preview again before applying."));
	}
	const FScopedTransaction Transaction(LOCTEXT("MaterializeTransaction", "Materialize DataForge Asset Layout Profile"));
	RuleSet.Modify();
	RuleSet.AssetRules = Candidate.AssetRules;
	RuleSet.GeneratedOutputs = Candidate.GeneratedOutputs;
	RuleSet.Bindings = Candidate.Bindings;
	RuleSet.ProfileOrigin = Candidate.ProfileOrigin;
#if WITH_EDITORONLY_DATA
	RuleSet.LastStatus = TEXT("Draft");
	RuleSet.LastSummary = TEXT("Asset Layout Profile materialized. Run Preview before Apply.");
	RuleSet.LastDiagnostics.Reset();
#endif
	RuleSet.MarkPackageDirty();
	RuleSet.PostEditChange();

	FDataForgeResult Result;
	Result.bSuccess = true;
	Result.Summary = Candidate.MakeSummary() + TEXT(" Applied. Concrete content has not been applied.");
	return Result;
}

bool FDataForgeAssetLayoutAuthoring::IsRebaseCandidateCurrent(
	const UDataForgeRuleSet& RuleSet,
	const FDataForgeAssetLayoutRebaseCandidate& Candidate)
{
	return Candidate.bSuccess && Candidate.SourceStateHash == ComputeRuleStateHash(RuleSet);
}

int32 FDataForgeAssetLayoutAuthoring::RefreshDependentStatuses(const UDataForgeAssetLayoutProfile* ChangedProfile)
{
	TArray<FAssetData> Assets;
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get()
		.GetAssetsByClass(UDataForgeRuleSet::StaticClass()->GetClassPathName(), Assets, true);
	int32 Refreshed = 0;
	for (const FAssetData& Asset : Assets)
	{
		UDataForgeRuleSet* RuleSet = Cast<UDataForgeRuleSet>(Asset.GetAsset());
		if (!RuleSet || !RuleSet->ProfileOrigin.IsSet()) continue;
		if (ChangedProfile && RuleSet->ProfileOrigin.ProfileId != ChangedProfile->ProfileId) continue;
		const FDataForgeAssetLayoutAnalysis Analysis = Analyze(*RuleSet);
#if WITH_EDITORONLY_DATA
		RuleSet->LastStatus = Analysis.Status == EDataForgeAssetLayoutStatus::Outdated ? TEXT("Profile Outdated")
			: Analysis.Status == EDataForgeAssetLayoutStatus::MissingProfile ? TEXT("Profile Missing")
			: TEXT("Draft");
		RuleSet->LastSummary = Analysis.MakeSummary() + TEXT(" Profile changes require explicit Rebase Preview and Apply.");
#endif
		++Refreshed;
	}
	return Refreshed;
}

void FDataForgeAssetLayoutAuthoring::Detach(UDataForgeRuleSet& RuleSet)
{
	if (!RuleSet.ProfileOrigin.IsSet()) return;
	const FScopedTransaction Transaction(LOCTEXT("DetachTransaction", "Detach DataForge Asset Layout Profile"));
	RuleSet.Modify();
	RuleSet.ProfileOrigin = FDataForgeProfileOrigin();
#if WITH_EDITORONLY_DATA
	RuleSet.LastStatus = TEXT("Draft");
	RuleSet.LastSummary = TEXT("Asset Layout Profile detached. Concrete Asset Rules were preserved as manual rules.");
#endif
	RuleSet.MarkPackageDirty();
	RuleSet.PostEditChange();
}

#undef LOCTEXT_NAMESPACE
