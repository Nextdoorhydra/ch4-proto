#include "DataForgeRenameAdvisor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "DataForgeBindingPreset.h"
#include "DataForgeFolderSource.h"
#include "DataForgeNamingPolicy.h"
#include "DataForgePipeline.h"
#include "DataForgeRuleSet.h"
#include "DataForgeRenameRecovery.h"
#include "Engine/DataAsset.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/MetaData.h"

namespace
{
	constexpr TCHAR FolderAdapterId[] = TEXT("AssetRegistryFolder");

	void AddDiagnostic(
		TArray<FDataForgeDiagnostic>& Diagnostics,
		EDataForgeSeverity Severity,
		const TCHAR* Code,
		const FString& Message,
		FName RecordId = NAME_None)
	{
		FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = Severity;
		Diagnostic.Code = Code;
		Diagnostic.Message = Message;
		Diagnostic.RecordId = RecordId;
	}

	FString NormalizeFolder(FString Folder)
	{
		Folder.ReplaceInline(TEXT("\\"), TEXT("/"));
		while (Folder.EndsWith(TEXT("/"))) Folder.LeftChopInline(1);
		return Folder;
	}

	bool IsWithinRoot(const FString& PackagePath, const FString& Root)
	{
		return PackagePath == Root || PackagePath.StartsWith(Root + TEXT("/"));
	}

	const FDataForgeAssociationSourceRule* ResolveAssociation(
		const UDataForgeRuleSet& RuleSet,
		const FDataForgeBindingPresetSlot& Slot)
	{
		if (!Slot.AssociationSourceId.IsNone())
		{
			return RuleSet.AssociationSources.FindByPredicate([&Slot](const FDataForgeAssociationSourceRule& Rule)
			{
				return Rule.SourceId == Slot.AssociationSourceId;
			});
		}
		return RuleSet.AssociationSources.Num() == 1 ? &RuleSet.AssociationSources[0] : nullptr;
	}

	const FDataForgeAssetKindNamingRule* FindKindRule(const UDataForgeNamingPolicy& Policy, FName AssetKind)
	{
		return Policy.AssetKinds.FindByPredicate([AssetKind](const FDataForgeAssetKindNamingRule& Rule)
		{
			return Rule.AssetKind == AssetKind;
		});
	}

	bool HasError(const TArray<FDataForgeDiagnostic>& Diagnostics)
	{
		return Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
		{
			return Diagnostic.Severity == EDataForgeSeverity::Error;
		});
	}

	FString MakeEvidenceKey(FName RecordId, FName SlotId)
	{
		return RecordId.ToString() + TEXT("|") + SlotId.ToString();
	}

	TMap<FSoftObjectPath, TSet<FString>> BuildManifestEvidence(
		const UDataForgeRuleSet& RuleSet,
		const TArray<FAssetData>& SelectedAssets)
	{
		TMap<FSoftObjectPath, TSet<FString>> Result;
		TSet<FString> SelectedPaths;
		for (const FAssetData& Asset : SelectedAssets) SelectedPaths.Add(Asset.GetSoftObjectPath().ToString());
		if (SelectedPaths.IsEmpty()) return Result;

		const UDataForgeBindingPreset* Preset = RuleSet.BindingPreset.LoadSynchronous();
		if (!Preset) return Result;
		TSet<FName> BaseFolders;
		for (const FDataForgeGeneratedAssetOutputRule& Output : RuleSet.GeneratedOutputs)
		{
			if (const FDataForgeAssetRule* Rule = RuleSet.AssetRules.FindByPredicate([&Output](const FDataForgeAssetRule& Candidate)
			{
				return Candidate.RuleId == Output.AssetRuleId;
			}))
			{
				if (!Rule->BaseFolder.IsEmpty()) BaseFolders.Add(FName(*NormalizeFolder(Rule->BaseFolder)));
			}
		}
		if (BaseFolders.IsEmpty()) return Result;

		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		for (const FName BaseFolder : BaseFolders)
		{
			TArray<FAssetData> ManagedAssets;
			Registry.GetAssetsByPath(BaseFolder, ManagedAssets, true, false);
			for (const FAssetData& ManagedData : ManagedAssets)
			{
				UDataAsset* ManagedAsset = Cast<UDataAsset>(ManagedData.GetAsset());
				if (!ManagedAsset) continue;
				FMetaData& MetaData = ManagedAsset->GetPackage()->GetMetaData();
				if (MetaData.GetValue(ManagedAsset, TEXT("DataForge.Managed")) != TEXT("true")
					|| MetaData.GetValue(ManagedAsset, TEXT("DataForge.RuleSetId")) != RuleSet.RuleSetId.ToString(EGuidFormats::Digits)) continue;
				const FName RecordId(*MetaData.GetValue(ManagedAsset, TEXT("DataForge.RecordId")));
				for (const FDataForgeBindingPresetSlot& Slot : Preset->Slots)
				{
					if (Slot.TargetProperty.IsEmpty()) continue;
					TArray<FString> AssociatedPaths;
					MetaData.GetValue(ManagedAsset, *(TEXT("DataForge.Association.") + Slot.TargetProperty))
						.ParseIntoArrayLines(AssociatedPaths, true);
					for (const FString& AssociatedPath : AssociatedPaths)
					{
						if (SelectedPaths.Contains(AssociatedPath))
						{
							Result.FindOrAdd(FSoftObjectPath(AssociatedPath)).Add(MakeEvidenceKey(RecordId, Slot.SlotId));
						}
					}
				}
			}
		}
		return Result;
	}

	FString BuildTargetFolder(
		const FString& Root,
		const FString& CurrentPackagePath,
		const UDataForgeAssetLayoutRecipe& Recipe,
		const FDataForgeAssetKindNamingRule& KindRule,
		const FString& SourceKey,
		TArray<FDataForgeDiagnostic>& Diagnostics)
	{
		FString Relative = CurrentPackagePath;
		Relative.RemoveFromStart(Root);
		Relative.RemoveFromStart(TEXT("/"));
		TArray<FString> Segments;
		Relative.ParseIntoArray(Segments, TEXT("/"), true);

		const int32 RequiredIndex = FMath::Max(
			Recipe.SubjectSource == EDataForgeLayoutSubjectSource::FolderSegment ? Recipe.SubjectFolderIndex : INDEX_NONE,
			Recipe.KindFolderIndex);
		while (Segments.Num() <= RequiredIndex) Segments.AddDefaulted();

		if (Recipe.SubjectSource == EDataForgeLayoutSubjectSource::FolderSegment)
		{
			Segments[Recipe.SubjectFolderIndex] = SourceKey;
		}
		if (Recipe.KindFolderIndex >= 0)
		{
			if (KindRule.FolderName.IsEmpty())
			{
				AddDiagnostic(Diagnostics, EDataForgeSeverity::Error, TEXT("DF1936"),
					TEXT("The selected Asset Kind has no folder name."));
			}
			else
			{
				Segments[Recipe.KindFolderIndex] = KindRule.FolderName;
			}
		}
		if (Segments.ContainsByPredicate([](const FString& Segment) { return Segment.IsEmpty(); }))
		{
			AddDiagnostic(Diagnostics, EDataForgeSeverity::Error, TEXT("DF1937"),
				TEXT("The layout recipe requires folder segments that cannot be inferred."));
			return FString();
		}
		return Segments.IsEmpty() ? Root : Root + TEXT("/") + FString::Join(Segments, TEXT("/"));
	}
}

FString FDataForgeRenameCandidate::GetSuggestedObjectPath() const
{
	return SuggestedPackageName.IsEmpty() || SuggestedAssetName.IsEmpty()
		? FString()
		: SuggestedPackageName + TEXT(".") + SuggestedAssetName;
}

bool FDataForgeRenameCandidate::HasErrors() const
{
	return HasError(Diagnostics);
}

bool FDataForgeRenameCandidate::IsChange() const
{
	return AssetPath.ToString() != GetSuggestedObjectPath();
}

TArray<FDataForgeRenameCandidate> FDataForgeRenameAdvisor::BuildCandidates(const FAssetData& AssetData)
{
	return BuildCandidates(TArray<FAssetData>{ AssetData });
}

TArray<FDataForgeRenameCandidate> FDataForgeRenameAdvisor::BuildCandidates(const TArray<FAssetData>& Assets)
{
	TArray<FDataForgeRenameCandidate> Result;
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> RuleSetAssets;
	Registry.GetAssetsByClass(UDataForgeRuleSet::StaticClass()->GetClassPathName(), RuleSetAssets, true);
	for (const FAssetData& RuleSetAsset : RuleSetAssets)
	{
		const UDataForgeRuleSet* RuleSet = Cast<UDataForgeRuleSet>(RuleSetAsset.GetAsset());
		if (!RuleSet) continue;
		const TSharedPtr<const IDataForgeSourceAdapter> PrimaryAdapter =
			FDataForgeSourceAdapterRegistry::Get().Find(RuleSet->Source.AdapterId);
		FDataForgeDataSet PrimaryData;
		TArray<FDataForgeDiagnostic> PrimaryDiagnostics;
		if (!PrimaryAdapter || !PrimaryAdapter->Fetch(RuleSet->Source, PrimaryData, PrimaryDiagnostics)) continue;
		const TMap<FSoftObjectPath, TSet<FString>> ManifestEvidence = BuildManifestEvidence(*RuleSet, Assets);
		for (const FAssetData& Asset : Assets)
		{
			Result.Append(BuildCandidatesForRuleSetWithData(
				Asset, *RuleSet, PrimaryData, ManifestEvidence.Find(Asset.GetSoftObjectPath())));
		}
	}
	TMap<FString, TArray<int32>> DestinationClaims;
	for (int32 Index = 0; Index < Result.Num(); ++Index)
	{
		if (!Result[Index].GetSuggestedObjectPath().IsEmpty())
		{
			DestinationClaims.FindOrAdd(Result[Index].GetSuggestedObjectPath()).Add(Index);
		}
	}
	for (const TPair<FString, TArray<int32>>& Claim : DestinationClaims)
	{
		TSet<FSoftObjectPath> Sources;
		for (const int32 Index : Claim.Value) Sources.Add(Result[Index].AssetPath);
		if (Sources.Num() < 2) continue;
		for (const int32 Index : Claim.Value)
		{
			AddDiagnostic(Result[Index].Diagnostics, EDataForgeSeverity::Error, TEXT("DF1947"),
				FString::Printf(TEXT("Multiple selected assets claim the same destination: %s"), *Claim.Key),
				Result[Index].RecordId);
		}
	}
	Result.Sort([](const FDataForgeRenameCandidate& A, const FDataForgeRenameCandidate& B)
	{
		if (A.AssetPath != B.AssetPath) return A.AssetPath.ToString() < B.AssetPath.ToString();
		return A.GetSuggestedObjectPath() < B.GetSuggestedObjectPath();
	});
	return Result;
}

TArray<FDataForgeRenameCandidate> FDataForgeRenameAdvisor::BuildCandidatesForRuleSet(
	const FAssetData& AssetData,
	const UDataForgeRuleSet& RuleSet)
{
	TArray<FDataForgeRenameCandidate> Result;
	const TSharedPtr<const IDataForgeSourceAdapter> PrimaryAdapter =
		FDataForgeSourceAdapterRegistry::Get().Find(RuleSet.Source.AdapterId);
	FDataForgeDataSet PrimaryData;
	TArray<FDataForgeDiagnostic> PrimaryDiagnostics;
	if (!PrimaryAdapter || !PrimaryAdapter->Fetch(RuleSet.Source, PrimaryData, PrimaryDiagnostics)) return Result;
	const TArray<FAssetData> Assets{ AssetData };
	const TMap<FSoftObjectPath, TSet<FString>> ManifestEvidence = BuildManifestEvidence(RuleSet, Assets);
	return BuildCandidatesForRuleSetWithData(
		AssetData, RuleSet, PrimaryData, ManifestEvidence.Find(AssetData.GetSoftObjectPath()));
}

TArray<FDataForgeRenameCandidate> FDataForgeRenameAdvisor::BuildCandidatesForRuleSetWithData(
	const FAssetData& AssetData,
	const UDataForgeRuleSet& RuleSet,
	const FDataForgeDataSet& PrimaryData,
	const TSet<FString>* ManifestMatches)
{
	TArray<FDataForgeRenameCandidate> Result;
	const UDataForgeBindingPreset* Preset = RuleSet.BindingPreset.LoadSynchronous();
	if (!Preset) return Result;
	const FString CurrentPackagePath = NormalizeFolder(FPackageName::GetLongPackagePath(AssetData.PackageName.ToString()));
	const FString CurrentAssetName = AssetData.AssetName.ToString();
	for (const FDataForgeBindingPresetSlot& Slot : Preset->Slots)
	{
		TArray<FDataForgeRenameCandidate> SlotCandidates;
		const FDataForgeAssociationSourceRule* Association = ResolveAssociation(RuleSet, Slot);
		if (!Association || Association->Source.AdapterId != FolderAdapterId) continue;
		const UDataForgeFolderSourceConfig* Config = Cast<UDataForgeFolderSourceConfig>(Association->Source.SourceAsset.LoadSynchronous());
		const UDataForgeAssetLayoutRecipe* Recipe = Config ? Config->LayoutRecipe.LoadSynchronous() : nullptr;
		const UDataForgeNamingPolicy* Policy = Recipe ? Recipe->NamingPolicy.LoadSynchronous() : nullptr;
		if (!Config || !Recipe || !Policy) continue;

		const FString Root = NormalizeFolder(Config->RootFolder);
		if (Root.IsEmpty() || !IsWithinRoot(CurrentPackagePath, Root)) continue;
		UClass* ExpectedClass = Slot.ExpectedAssetClass.LoadSynchronous();
		if (ExpectedClass && !AssetData.IsInstanceOf(ExpectedClass)) continue;
		const FDataForgeAssetKindNamingRule* KindRule = FindKindRule(*Policy, Slot.AssetKind);
		if (!KindRule) continue;
		FString RelativeFolder = CurrentPackagePath;
		RelativeFolder.RemoveFromStart(Root);
		RelativeFolder.RemoveFromStart(TEXT("/"));
		TArray<FString> CurrentSegments;
		RelativeFolder.ParseIntoArray(CurrentSegments, TEXT("/"), true);

		const FName KeyColumn = Slot.SourceKeyColumn.IsNone() ? RuleSet.Schema.PrimaryKey : Slot.SourceKeyColumn;
		if (KeyColumn.IsNone()) continue;
		for (const FDataForgeRow& Row : PrimaryData.Rows)
		{
			const FString* RawKey = Row.Values.Find(KeyColumn);
			const FString SourceKey = RawKey ? RawKey->TrimStartAndEnd() : FString();
			if (SourceKey.IsEmpty()) continue;

			FDataForgeRenameCandidate Candidate;
			Candidate.AssetPath = AssetData.GetSoftObjectPath();
			Candidate.RuleSetPath = FSoftObjectPath(&RuleSet);
			Candidate.RuleSet = &RuleSet;
			Candidate.SlotId = Slot.SlotId;
			Candidate.RecordId = FName(*SourceKey);
			Candidate.Reason = FString::Printf(TEXT("RuleSet %s / record %s / slot %s"),
				*RuleSet.GetName(), *SourceKey, *Slot.SlotId.ToString());
			if (ManifestMatches && ManifestMatches->Contains(MakeEvidenceKey(Candidate.RecordId, Candidate.SlotId)))
			{
				Candidate.MatchScore += 1000;
				Candidate.MatchEvidence.Add(TEXT("Existing Association Manifest"));
			}
			if (Recipe->SubjectSource == EDataForgeLayoutSubjectSource::FolderSegment
				&& CurrentSegments.IsValidIndex(Recipe->SubjectFolderIndex)
				&& CurrentSegments[Recipe->SubjectFolderIndex] == SourceKey)
			{
				Candidate.MatchScore += 250;
				Candidate.MatchEvidence.Add(TEXT("Subject folder equals source key"));
			}

			FDataForgeAssetIdentity Identity;
			Identity.AssetKind = Slot.AssetKind;
			Identity.Subject = Recipe->SubjectSource == EDataForgeLayoutSubjectSource::Fixed
				? Recipe->FixedSubject
				: SourceKey;
			Identity.Role = Recipe->SubjectSource == EDataForgeLayoutSubjectSource::Fixed
				? SourceKey + Slot.Role.ToString()
				: Slot.Role.ToString();
			const FDataForgeNamingResult Naming = FDataForgeNamingPolicyResolver::Build(*Policy, Identity);
			Candidate.Diagnostics.Append(Naming.Diagnostics);
			if (Naming.bSuccess)
			{
				Candidate.SuggestedAssetName = Naming.AssetName;
				if (CurrentAssetName == Naming.AssetName)
				{
					Candidate.MatchScore += 300;
					Candidate.MatchEvidence.Add(TEXT("Current name already matches the rule"));
				}
				FDataForgeNamingParseContext ParseContext;
				ParseContext.AssetKind = Slot.AssetKind;
				ParseContext.Subject = Identity.Subject;
				ParseContext.Role = Identity.Role;
				ParseContext.ActualAssetClass = AssetData.GetClass(EResolveClass::Yes);
				if (FDataForgeNamingPolicyResolver::Parse(*Policy, CurrentAssetName, ParseContext).bSuccess)
				{
					Candidate.MatchScore += 200;
					Candidate.MatchEvidence.Add(TEXT("Current name parses to this record and slot"));
				}
				const FString TargetFolder = BuildTargetFolder(
					Root, CurrentPackagePath, *Recipe, *KindRule, SourceKey, Candidate.Diagnostics);
				if (!TargetFolder.IsEmpty())
				{
					Candidate.SuggestedPackageName = TargetFolder + TEXT("/") + Naming.AssetName;
				}
			}

			if (!Candidate.SuggestedPackageName.IsEmpty()
				&& !FPackageName::IsValidLongPackageName(Candidate.SuggestedPackageName))
			{
				AddDiagnostic(Candidate.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1938"),
					TEXT("The inferred destination is not a valid Unreal package name."), Candidate.RecordId);
			}
			const FAssetData Existing = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"))
				.Get().GetAssetByObjectPath(FSoftObjectPath(Candidate.GetSuggestedObjectPath()));
			if (Existing.IsValid() && Existing.GetSoftObjectPath() != Candidate.AssetPath)
			{
				AddDiagnostic(Candidate.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1939"),
					FString::Printf(TEXT("Destination already exists: %s"), *Candidate.GetSuggestedObjectPath()), Candidate.RecordId);
			}
			SlotCandidates.Add(MoveTemp(Candidate));
		}

		int32 HighestScore = 0;
		for (const FDataForgeRenameCandidate& Candidate : SlotCandidates)
		{
			HighestScore = FMath::Max(HighestScore, Candidate.MatchScore);
		}
		if (HighestScore < 100) continue;
		TArray<int32> Highest;
		for (int32 Index = 0; Index < SlotCandidates.Num(); ++Index)
		{
			if (SlotCandidates[Index].MatchScore == HighestScore) Highest.Add(Index);
		}
		if (Highest.Num() == 1)
		{
			FDataForgeRenameCandidate& Recommended = SlotCandidates[Highest[0]];
			Recommended.bRecommended = true;
			AddDiagnostic(Recommended.Diagnostics, EDataForgeSeverity::Info, TEXT("DF1956"),
				FString::Printf(TEXT("Recommended with score %d: %s"),
					Recommended.MatchScore, *FString::Join(Recommended.MatchEvidence, TEXT(", "))),
				Recommended.RecordId);
			Result.Add(MoveTemp(Recommended));
		}
		else
		{
			for (const int32 Index : Highest)
			{
				FDataForgeRenameCandidate& Ambiguous = SlotCandidates[Index];
				AddDiagnostic(Ambiguous.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1955"),
					FString::Printf(TEXT("%d records share the highest match score (%d). DataForge will not guess."),
						Highest.Num(), HighestScore), Ambiguous.RecordId);
				Result.Add(MoveTemp(Ambiguous));
			}
		}
	}
	return Result;
}

FDataForgeResult FDataForgeRenameAdvisor::ValidateForApply(const FDataForgeRenameCandidate& Candidate)
{
	FDataForgeResult Result;
	Result.bSuccess = false;
	Result.Diagnostics = Candidate.Diagnostics;
	if (Candidate.HasErrors()) return Result;
	if (!Candidate.IsChange())
	{
		AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1940"),
			TEXT("The asset already has the proposed name and location."));
		return Result;
	}
	UObject* Asset = Candidate.AssetPath.ResolveObject();
	if (!Asset && FPackageName::DoesPackageExist(Candidate.AssetPath.GetLongPackageName()))
	{
		Asset = Candidate.AssetPath.TryLoad();
	}
	if (!Asset || FSoftObjectPath(Asset) != Candidate.AssetPath)
	{
		AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1941"),
			TEXT("The candidate is stale because the source asset no longer exists at its original path."));
		return Result;
	}
	if (!FPackageName::IsValidLongPackageName(Candidate.SuggestedPackageName))
	{
		AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1942"),
			TEXT("The destination package is invalid."));
		return Result;
	}
	const UDataForgeRuleSet* RuleSet = Candidate.RuleSet.Get();
	if (!RuleSet && !Candidate.RuleSetPath.IsNull()) RuleSet = Cast<UDataForgeRuleSet>(Candidate.RuleSetPath.TryLoad());
	if (!RuleSet)
	{
		AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1945"),
			TEXT("The candidate is stale because its RuleSet is no longer available."));
		return Result;
	}
	const TArray<FDataForgeRenameCandidate> FreshCandidates = BuildCandidatesForRuleSet(FAssetData(Asset), *RuleSet);
	const bool bStillDefined = FreshCandidates.ContainsByPredicate([&Candidate](const FDataForgeRenameCandidate& Fresh)
	{
		return Fresh.RecordId == Candidate.RecordId
			&& Fresh.SlotId == Candidate.SlotId
			&& Fresh.GetSuggestedObjectPath() == Candidate.GetSuggestedObjectPath();
	});
	if (!bStillDefined)
	{
		AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1946"),
			TEXT("The source data or layout configuration changed after this candidate was created. Refresh the advisor."));
		return Result;
	}
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	const FAssetData Existing = Registry.GetAssetByObjectPath(FSoftObjectPath(Candidate.GetSuggestedObjectPath()));
	if (Existing.IsValid() && Existing.GetSoftObjectPath() != Candidate.AssetPath)
	{
		AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1943"),
			TEXT("The destination became occupied after the candidate was created."));
		return Result;
	}
	Result.bSuccess = true;
	return Result;
}

FDataForgeResult FDataForgeRenameAdvisor::Apply(const FDataForgeRenameCandidate& Candidate)
{
	return ApplyBatch(TArray<FDataForgeRenameCandidate>{ Candidate });
}

FDataForgeResult FDataForgeRenameAdvisor::ValidateBatchForApply(const TArray<FDataForgeRenameCandidate>& Candidates)
{
	FDataForgeResult Result;
	if (Candidates.IsEmpty())
	{
		AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1948"),
			TEXT("Select at least one rename candidate."));
		return Result;
	}

	TSet<FSoftObjectPath> Sources;
	TSet<FString> Destinations;
	for (const FDataForgeRenameCandidate& Candidate : Candidates)
	{
		if (Sources.Contains(Candidate.AssetPath))
		{
			AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1949"),
				FString::Printf(TEXT("More than one candidate was selected for %s."), *Candidate.AssetPath.ToString()));
		}
		Sources.Add(Candidate.AssetPath);

		const FString Destination = Candidate.GetSuggestedObjectPath();
		if (Destinations.Contains(Destination))
		{
			AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1950"),
				FString::Printf(TEXT("More than one selected asset targets %s."), *Destination));
		}
		Destinations.Add(Destination);
	}
	if (HasError(Result.Diagnostics)) return Result;

	for (const FDataForgeRenameCandidate& Candidate : Candidates)
	{
		const FDataForgeResult CandidateResult = ValidateForApply(Candidate);
		Result.Diagnostics.Append(CandidateResult.Diagnostics);
		if (!CandidateResult.bSuccess) return Result;
	}
	Result.bSuccess = true;
	Result.Summary = FString::Printf(TEXT("%d rename candidate(s) are ready."), Candidates.Num());
	return Result;
}

FDataForgeResult FDataForgeRenameAdvisor::ApplyBatch(const TArray<FDataForgeRenameCandidate>& Candidates)
{
	FDataForgeResult Result = ValidateBatchForApply(Candidates);
	if (!Result.bSuccess) return Result;
	FDataForgeRenameRecoveryRecord Recovery;
	FString RecoveryError;
	if (!FDataForgeRenameRecovery::Begin(Candidates, Recovery, RecoveryError))
	{
		Result.bSuccess = false;
		AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1952"), RecoveryError);
		Result.Summary = TEXT("Rename batch was not started because its recovery manifest could not be written.");
		return Result;
	}

	TArray<FAssetRenameData> RenameData;
	TArray<UObject*> Assets;
	RenameData.Reserve(Candidates.Num());
	Assets.Reserve(Candidates.Num());
	for (const FDataForgeRenameCandidate& Candidate : Candidates)
	{
		UObject* Asset = Candidate.AssetPath.ResolveObject();
		Assets.Add(Asset);
		RenameData.Emplace(
			Asset,
			FPackageName::GetLongPackagePath(Candidate.SuggestedPackageName),
			Candidate.SuggestedAssetName);
	}
	if (!FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().RenameAssets(RenameData))
	{
		Result.bSuccess = false;
		AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1951"),
			TEXT("Unreal AssetTools rejected the batch rename operation."));

		TArray<FAssetRenameData> RollbackData;
		bool bRollbackSucceeded = true;
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			const FString CurrentPath = Assets[Index] ? Assets[Index]->GetPathName() : FString();
			if (CurrentPath == Candidates[Index].AssetPath.ToString()) continue;
			if (CurrentPath != Candidates[Index].GetSuggestedObjectPath())
			{
				bRollbackSucceeded = false;
				continue;
			}
			RollbackData.Emplace(
				Assets[Index],
				FPackageName::GetLongPackagePath(Candidates[Index].AssetPath.GetLongPackageName()),
				FPackageName::ObjectPathToObjectName(Candidates[Index].AssetPath.ToString()));
		}
		const bool bRollbackAttempted = !RollbackData.IsEmpty();
		if (bRollbackAttempted
			&& !FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().RenameAssets(RollbackData))
		{
			bRollbackSucceeded = false;
		}
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			bRollbackSucceeded &= Assets[Index] && Assets[Index]->GetPathName() == Candidates[Index].AssetPath.ToString();
		}

		FString UpdateError;
		const FString FailureMessage = bRollbackSucceeded
			? TEXT("AssetTools failed; every observed move was restored to its source path.")
			: TEXT("AssetTools failed and DataForge could not restore every asset. Inspect the recorded paths before continuing.");
		if (!FDataForgeRenameRecovery::Update(
			Recovery,
			bRollbackSucceeded ? TEXT("FailedRolledBack") : TEXT("FailedRollbackIncomplete"),
			bRollbackAttempted,
			bRollbackSucceeded,
			FailureMessage,
			UpdateError))
		{
			AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Warning, TEXT("DF1953"), UpdateError);
		}
		Result.Summary = FailureMessage + TEXT(" Recovery: ") + Recovery.Filename;
		return Result;
	}

	FString UpdateError;
	if (!FDataForgeRenameRecovery::Update(
		Recovery,
		TEXT("Succeeded"),
		false,
		false,
		FString::Printf(TEXT("Renamed %d asset(s)."), Candidates.Num()),
		UpdateError))
	{
		AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Warning, TEXT("DF1953"), UpdateError);
	}
	Result.Summary = FString::Printf(TEXT("Renamed %d asset(s). Recovery: %s"), Candidates.Num(), *Recovery.Filename);
	return Result;
}
