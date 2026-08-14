#include "DataForgeSourceFolderAnalyzer.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DataForgeNamingPolicy.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"

namespace DataForgeSourceFolderAnalyzer
{
	constexpr float MinimumCoverage = 0.5f;
	constexpr float EqualScoreTolerance = 0.0001f;

	void AddDiagnostic(
		TArray<FDataForgeDiagnostic>& Diagnostics,
		EDataForgeSeverity Severity,
		const TCHAR* Code,
		const FString& Message,
		FName Field = NAME_None)
	{
		FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = Severity;
		Diagnostic.Code = Code;
		Diagnostic.Message = Message;
		Diagnostic.Field = Field;
	}

	FString NormalizeFolder(FString Folder)
	{
		Folder.ReplaceInline(TEXT("\\"), TEXT("/"));
		while (Folder.EndsWith(TEXT("/"))) Folder.LeftChopInline(1);
		return Folder;
	}

	TArray<FString> RelativeSegments(const FString& RootFolder, const FString& PackagePath)
	{
		const FString NormalizedPath = NormalizeFolder(PackagePath);
		FString Relative;
		if (NormalizedPath.StartsWith(RootFolder + TEXT("/"), ESearchCase::CaseSensitive))
		{
			Relative = NormalizedPath.Mid(RootFolder.Len() + 1);
		}
		TArray<FString> Segments;
		Relative.ParseIntoArray(Segments, TEXT("/"), true);
		return Segments;
	}

	int32 PrimaryKeyNamePriority(FName Column)
	{
		const FString Name = Column.ToString();
		if (Name.Equals(TEXT("Id"), ESearchCase::IgnoreCase)) return 100;
		if (Name.EndsWith(TEXT("Id"), ESearchCase::IgnoreCase)) return 80;
		if (Name.Equals(TEXT("Key"), ESearchCase::IgnoreCase) || Name.EndsWith(TEXT("Key"), ESearchCase::IgnoreCase)) return 60;
		if (Name.Equals(TEXT("Name"), ESearchCase::IgnoreCase)) return 20;
		return 0;
	}

	bool KindFolderMatches(const FString& Segment, FName AssetKind)
	{
		const FString Kind = AssetKind.ToString();
		return Segment.Equals(Kind, ESearchCase::IgnoreCase)
			|| Segment.Equals(Kind + TEXT("s"), ESearchCase::IgnoreCase);
	}

	FString BuildPattern(int32 SubjectIndex, int32 KindIndex)
	{
		const int32 SegmentCount = FMath::Max(SubjectIndex, KindIndex) + 1;
		TArray<FString> Segments;
		Segments.Init(TEXT("*"), SegmentCount);
		Segments[SubjectIndex] = TEXT("{Subject}");
		Segments[KindIndex] = TEXT("{AssetKind}");
		return FString::Join(Segments, TEXT("/"));
	}

	FName InferKind(const FAssetData& AssetData, const UDataForgeNamingPolicy* NamingPolicy)
	{
		const FString AssetName = AssetData.AssetName.ToString();
		if (NamingPolicy)
		{
			for (const FDataForgeAssetKindNamingRule& Rule : NamingPolicy->AssetKinds)
			{
				UClass* ExpectedClass = Rule.ExpectedAssetClass.LoadSynchronous();
				if (ExpectedClass && AssetData.IsInstanceOf(ExpectedClass)) return Rule.AssetKind;
			}
		}

		const FString ClassName = AssetData.AssetClassPath.GetAssetName().ToString();
		if (ClassName.Contains(TEXT("Texture"), ESearchCase::IgnoreCase)) return TEXT("Texture");
		if (ClassName.Contains(TEXT("Material"), ESearchCase::IgnoreCase)) return TEXT("Material");
		if (ClassName.Contains(TEXT("Mesh"), ESearchCase::IgnoreCase)) return TEXT("Mesh");
		if (ClassName.Contains(TEXT("Niagara"), ESearchCase::IgnoreCase)) return TEXT("Niagara");
		if (ClassName.Contains(TEXT("Blueprint"), ESearchCase::IgnoreCase)) return TEXT("Blueprint");
		if (NamingPolicy)
		{
			for (const FDataForgeAssetKindNamingRule& Rule : NamingPolicy->AssetKinds)
			{
				if (!Rule.TypePrefix.IsEmpty() && AssetName.StartsWith(Rule.TypePrefix + TEXT("_"))) return Rule.AssetKind;
			}
		}
		if (AssetName.StartsWith(TEXT("T_"))) return TEXT("Texture");
		if (AssetName.StartsWith(TEXT("M_")) || AssetName.StartsWith(TEXT("MI_"))) return TEXT("Material");
		if (AssetName.StartsWith(TEXT("SM_")) || AssetName.StartsWith(TEXT("SK_"))) return TEXT("Mesh");
		if (AssetName.StartsWith(TEXT("NS_"))) return TEXT("Niagara");
		if (AssetName.StartsWith(TEXT("BP_"))) return TEXT("Blueprint");
		return NAME_None;
	}
}

FDataForgePrimaryKeyAnalysis FDataForgeSourceFolderAnalyzer::AnalyzePrimaryKey(
	const FDataForgeDataSet& DataSet,
	FName PreferredPrimaryKey)
{
	FDataForgePrimaryKeyAnalysis Analysis;
	Analysis.Decision.DecisionId = TEXT("PrimaryKey");
	Analysis.Decision.bRequired = true;
	for (const FName Column : DataSet.Columns)
	{
		FDataForgePrimaryKeyCandidate& Candidate = Analysis.Candidates.AddDefaulted_GetRef();
		Candidate.Column = Column;
		Candidate.NamePriority = DataForgeSourceFolderAnalyzer::PrimaryKeyNamePriority(Column);
		TSet<FString> UniqueValues;
		for (const FDataForgeRow& Row : DataSet.Rows)
		{
			const FString Value = Row.Values.FindRef(Column).TrimStartAndEnd();
			if (Value.IsEmpty()) continue;
			++Candidate.NonEmptyCount;
			UniqueValues.Add(Value);
		}
		Candidate.UniqueCount = UniqueValues.Num();
		if (!DataSet.Rows.IsEmpty())
		{
			Candidate.NonEmptyRatio = static_cast<float>(Candidate.NonEmptyCount) / DataSet.Rows.Num();
			Candidate.UniqueRatio = static_cast<float>(Candidate.UniqueCount) / DataSet.Rows.Num();
		}
	}

	Analysis.Candidates.Sort([](const FDataForgePrimaryKeyCandidate& A, const FDataForgePrimaryKeyCandidate& B)
	{
		if (A.NonEmptyRatio != B.NonEmptyRatio) return A.NonEmptyRatio > B.NonEmptyRatio;
		if (A.UniqueRatio != B.UniqueRatio) return A.UniqueRatio > B.UniqueRatio;
		if (A.NamePriority != B.NamePriority) return A.NamePriority > B.NamePriority;
		return A.Column.LexicalLess(B.Column);
	});
	for (const FDataForgePrimaryKeyCandidate& Candidate : Analysis.Candidates)
	{
		Analysis.Decision.Alternatives.Add(Candidate.Column.ToString());
	}

	if (!PreferredPrimaryKey.IsNone())
	{
		const FDataForgePrimaryKeyCandidate* Preferred = Analysis.Candidates.FindByPredicate([PreferredPrimaryKey](const FDataForgePrimaryKeyCandidate& Candidate)
		{
			return Candidate.Column == PreferredPrimaryKey;
		});
		if (!Preferred)
		{
			Analysis.Decision.Disposition = EDataForgeInferenceDisposition::Conflict;
			DataForgeSourceFolderAnalyzer::AddDiagnostic(Analysis.Diagnostics, EDataForgeSeverity::Error, TEXT("DF2001"),
				FString::Printf(TEXT("Preferred Primary Key '%s' is not present in the source."), *PreferredPrimaryKey.ToString()), PreferredPrimaryKey);
			return Analysis;
		}
		Analysis.Decision.SelectedValue = PreferredPrimaryKey.ToString();
		if (Preferred->NonEmptyRatio == 1.0f && Preferred->UniqueRatio == 1.0f)
		{
			Analysis.Decision.Disposition = EDataForgeInferenceDisposition::Exact;
		}
		else
		{
			Analysis.Decision.Disposition = EDataForgeInferenceDisposition::Conflict;
			DataForgeSourceFolderAnalyzer::AddDiagnostic(Analysis.Diagnostics, EDataForgeSeverity::Error, TEXT("DF2002"),
				FString::Printf(TEXT("Preferred Primary Key '%s' contains empty or duplicate values."), *PreferredPrimaryKey.ToString()), PreferredPrimaryKey);
		}
		return Analysis;
	}

	TArray<const FDataForgePrimaryKeyCandidate*> Viable;
	for (const FDataForgePrimaryKeyCandidate& Candidate : Analysis.Candidates)
	{
		if (Candidate.NonEmptyRatio == 1.0f && Candidate.UniqueRatio == 1.0f) Viable.Add(&Candidate);
	}
	if (Viable.IsEmpty())
	{
		Analysis.Decision.Disposition = EDataForgeInferenceDisposition::Unsupported;
		DataForgeSourceFolderAnalyzer::AddDiagnostic(Analysis.Diagnostics, EDataForgeSeverity::Error, TEXT("DF2003"),
			TEXT("No source column is both complete and unique enough to use as a Primary Key."));
		return Analysis;
	}

	const int32 BestPriority = Viable[0]->NamePriority;
	const int32 EqualBestCount = Viable.FilterByPredicate([BestPriority](const FDataForgePrimaryKeyCandidate* Candidate)
	{
		return Candidate->NamePriority == BestPriority;
	}).Num();
	if (EqualBestCount > 1)
	{
		Analysis.Decision.Disposition = EDataForgeInferenceDisposition::Ambiguous;
		DataForgeSourceFolderAnalyzer::AddDiagnostic(Analysis.Diagnostics, EDataForgeSeverity::Warning, TEXT("DF2004"),
			FString::Printf(TEXT("%d complete and unique columns have equal Primary Key priority."), EqualBestCount));
		return Analysis;
	}

	Analysis.Decision.SelectedValue = Viable[0]->Column.ToString();
	Analysis.Decision.Disposition = BestPriority == 100
		? EDataForgeInferenceDisposition::Exact
		: EDataForgeInferenceDisposition::Recommended;
	FDataForgeInferenceEvidence& Evidence = Analysis.Decision.Evidence.AddDefaulted_GetRef();
	Evidence.Kind = TEXT("UniqueValues");
	Evidence.Value = TEXT("100%");
	Evidence.Source = TEXT("Source Probe");
	return Analysis;
}

FDataForgeFolderLayoutAnalysis FDataForgeSourceFolderAnalyzer::AnalyzeFolderLayout(
	const FString& RootFolder,
	const FDataForgeDataSet& PrimaryData,
	FName SourceKeyColumn,
	const TArray<FDataForgeFolderAssetObservation>& Observations)
{
	FDataForgeFolderLayoutAnalysis Analysis;
	Analysis.RootFolder = DataForgeSourceFolderAnalyzer::NormalizeFolder(RootFolder);
	Analysis.Decision.DecisionId = TEXT("FolderLayout");
	Analysis.Decision.bRequired = true;
	if (!FPackageName::IsValidLongPackageName(Analysis.RootFolder) || SourceKeyColumn.IsNone())
	{
		Analysis.Decision.Disposition = EDataForgeInferenceDisposition::Unsupported;
		DataForgeSourceFolderAnalyzer::AddDiagnostic(Analysis.Diagnostics, EDataForgeSeverity::Error, TEXT("DF2010"),
			TEXT("Folder layout analysis requires a valid Content root and Source Key column."));
		return Analysis;
	}

	TSet<FString> SourceKeys;
	for (const FDataForgeRow& Row : PrimaryData.Rows)
	{
		const FString Key = Row.Values.FindRef(SourceKeyColumn).TrimStartAndEnd();
		if (!Key.IsEmpty()) SourceKeys.Add(Key);
	}
	if (SourceKeys.IsEmpty() || Observations.IsEmpty())
	{
		Analysis.Decision.Disposition = EDataForgeInferenceDisposition::Unsupported;
		DataForgeSourceFolderAnalyzer::AddDiagnostic(Analysis.Diagnostics, EDataForgeSeverity::Error, TEXT("DF2011"),
			TEXT("Folder layout analysis requires source keys and classified asset observations."));
		return Analysis;
	}

	TArray<TArray<FString>> SegmentsByObservation;
	int32 MaximumDepth = 0;
	for (const FDataForgeFolderAssetObservation& Observation : Observations)
	{
		TArray<FString>& Segments = SegmentsByObservation.Add_GetRef(
			DataForgeSourceFolderAnalyzer::RelativeSegments(Analysis.RootFolder, Observation.PackagePath));
		MaximumDepth = FMath::Max(MaximumDepth, Segments.Num());
	}

	for (int32 SubjectIndex = 0; SubjectIndex < MaximumDepth; ++SubjectIndex)
	{
		int32 SubjectMatches = 0;
		TSet<FString> MatchedKeys;
		for (const TArray<FString>& Segments : SegmentsByObservation)
		{
			if (Segments.IsValidIndex(SubjectIndex) && SourceKeys.Contains(Segments[SubjectIndex]))
			{
				++SubjectMatches;
				MatchedKeys.Add(Segments[SubjectIndex]);
			}
		}
		const float SubjectCoverage = static_cast<float>(SubjectMatches) / Observations.Num();
		const float KeyCoverage = static_cast<float>(MatchedKeys.Num()) / SourceKeys.Num();
		if (SubjectCoverage < DataForgeSourceFolderAnalyzer::MinimumCoverage) continue;

		for (int32 KindIndex = 0; KindIndex < MaximumDepth; ++KindIndex)
		{
			if (KindIndex == SubjectIndex) continue;
			int32 ClassifiedCount = 0;
			int32 KindMatches = 0;
			for (int32 ObservationIndex = 0; ObservationIndex < Observations.Num(); ++ObservationIndex)
			{
				const FName Kind = Observations[ObservationIndex].AssetKind;
				if (Kind.IsNone()) continue;
				++ClassifiedCount;
				const TArray<FString>& Segments = SegmentsByObservation[ObservationIndex];
				if (Segments.IsValidIndex(KindIndex)
					&& DataForgeSourceFolderAnalyzer::KindFolderMatches(Segments[KindIndex], Kind))
				{
					++KindMatches;
				}
			}
			if (ClassifiedCount == 0) continue;
			const float KindCoverage = static_cast<float>(KindMatches) / ClassifiedCount;
			if (KindCoverage < DataForgeSourceFolderAnalyzer::MinimumCoverage) continue;

			FDataForgeFolderLayoutCandidate& Candidate = Analysis.Candidates.AddDefaulted_GetRef();
			Candidate.SubjectFolderIndex = SubjectIndex;
			Candidate.KindFolderIndex = KindIndex;
			Candidate.SubjectCoverage = SubjectCoverage;
			Candidate.SourceKeyCoverage = KeyCoverage;
			Candidate.KindCoverage = KindCoverage;
			Candidate.CombinedScore = SubjectCoverage * 0.5f + KeyCoverage * 0.25f + KindCoverage * 0.25f;
			Candidate.Pattern = DataForgeSourceFolderAnalyzer::BuildPattern(SubjectIndex, KindIndex);
		}
	}

	Analysis.Candidates.Sort([](const FDataForgeFolderLayoutCandidate& A, const FDataForgeFolderLayoutCandidate& B)
	{
		if (!FMath::IsNearlyEqual(A.CombinedScore, B.CombinedScore)) return A.CombinedScore > B.CombinedScore;
		if (A.SubjectFolderIndex != B.SubjectFolderIndex) return A.SubjectFolderIndex < B.SubjectFolderIndex;
		return A.KindFolderIndex < B.KindFolderIndex;
	});
	for (const FDataForgeFolderLayoutCandidate& Candidate : Analysis.Candidates) Analysis.Decision.Alternatives.Add(Candidate.Pattern);
	if (Analysis.Candidates.IsEmpty())
	{
		Analysis.Decision.Disposition = EDataForgeInferenceDisposition::Unsupported;
		DataForgeSourceFolderAnalyzer::AddDiagnostic(Analysis.Diagnostics, EDataForgeSeverity::Warning, TEXT("DF2012"),
			TEXT("No folder pattern reaches the minimum Subject and Asset Kind coverage."));
		return Analysis;
	}

	const FDataForgeFolderLayoutCandidate& Best = Analysis.Candidates[0];
	const bool bTied = Analysis.Candidates.Num() > 1
		&& FMath::Abs(Analysis.Candidates[1].CombinedScore - Best.CombinedScore) <= DataForgeSourceFolderAnalyzer::EqualScoreTolerance;
	if (bTied)
	{
		Analysis.Decision.Disposition = EDataForgeInferenceDisposition::Ambiguous;
		DataForgeSourceFolderAnalyzer::AddDiagnostic(Analysis.Diagnostics, EDataForgeSeverity::Warning, TEXT("DF2013"),
			TEXT("Multiple folder patterns have equal evidence. Select one explicitly."));
		return Analysis;
	}

	Analysis.Decision.SelectedValue = Best.Pattern;
	Analysis.Decision.Disposition = Best.SubjectCoverage == 1.0f
		&& Best.SourceKeyCoverage == 1.0f
		&& Best.KindCoverage == 1.0f
		? EDataForgeInferenceDisposition::Exact
		: EDataForgeInferenceDisposition::Recommended;
	Analysis.Decision.Evidence = {
		{ TEXT("SubjectCoverage"), FString::Printf(TEXT("%.0f%%"), Best.SubjectCoverage * 100.0f), TEXT("Asset Registry") },
		{ TEXT("SourceKeyCoverage"), FString::Printf(TEXT("%.0f%%"), Best.SourceKeyCoverage * 100.0f), TEXT("Source Probe") },
		{ TEXT("KindCoverage"), FString::Printf(TEXT("%.0f%%"), Best.KindCoverage * 100.0f), TEXT("Asset Registry") }
	};
	return Analysis;
}

bool FDataForgeSourceFolderAnalyzer::ScanFolder(
	const FString& RootFolder,
	const UDataForgeNamingPolicy* NamingPolicy,
	TArray<FDataForgeFolderAssetObservation>& OutObservations,
	TArray<FDataForgeDiagnostic>& OutDiagnostics)
{
	OutObservations.Reset();
	const FString Root = DataForgeSourceFolderAnalyzer::NormalizeFolder(RootFolder);
	if (!FPackageName::IsValidLongPackageName(Root))
	{
		DataForgeSourceFolderAnalyzer::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF2020"),
			FString::Printf(TEXT("Asset search root '%s' is not a valid Content path."), *Root));
		return false;
	}

	TArray<FAssetData> Assets;
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.GetAssetsByPath(FName(*Root), Assets, true, false);
	Assets.Sort([](const FAssetData& A, const FAssetData& B)
	{
		return A.GetSoftObjectPath().ToString() < B.GetSoftObjectPath().ToString();
	});
	for (const FAssetData& Asset : Assets)
	{
		if (Asset.IsRedirector()) continue;
		const FName Kind = DataForgeSourceFolderAnalyzer::InferKind(Asset, NamingPolicy);
		if (Kind.IsNone()) continue;
		FDataForgeFolderAssetObservation& Observation = OutObservations.AddDefaulted_GetRef();
		Observation.ObjectPath = Asset.GetSoftObjectPath().ToString();
		Observation.PackagePath = Asset.PackagePath.ToString();
		Observation.AssetClassPath = Asset.AssetClassPath.ToString();
		Observation.AssetKind = Kind;
	}
	if (OutObservations.IsEmpty())
	{
		DataForgeSourceFolderAnalyzer::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Warning, TEXT("DF2021"),
			TEXT("The asset search root contains no supported Texture, Material, Mesh, Niagara, or Blueprint assets."));
	}
	return true;
}
