#include "DataForgeAuthoringPlanner.h"

#include "Misc/PackageName.h"

namespace DataForgeAuthoringPlanner
{
	FString NormalizeFolder(FString Folder)
	{
		Folder.TrimStartAndEndInline();
		Folder.ReplaceInline(TEXT("\\"), TEXT("/"));
		while (Folder.EndsWith(TEXT("/"))) Folder.LeftChopInline(1);
		return Folder;
	}

	void AppendDiagnostics(TArray<FDataForgeDiagnostic>& Target, const TArray<FDataForgeDiagnostic>& Source)
	{
		Target.Append(Source);
	}

	FDataForgeAuthoringDecision MakeFolderUnavailableDecision(const FString& RootFolder)
	{
		FDataForgeAuthoringDecision Decision;
		Decision.DecisionId = FName(*FString::Printf(TEXT("FolderLayout.%s"), *RootFolder));
		Decision.Disposition = EDataForgeInferenceDisposition::Unsupported;
		Decision.bRequired = true;
		return Decision;
	}

	void ApplyExplicitRowReference(
		FDataForgeOutputReflectionAnalysis& Analysis,
		const FDataForgeRequestedOutput& Requested,
		TArray<FDataForgeDiagnostic>& Diagnostics)
	{
		if (Requested.RowReferenceProperty.IsEmpty()) return;
		const FString* Compatible = Analysis.RowReferenceDecision.Alternatives.FindByPredicate([&Requested](const FString& Candidate)
		{
			return Candidate.Equals(Requested.RowReferenceProperty, ESearchCase::IgnoreCase);
		});
		if (Compatible)
		{
			Analysis.RowReferenceDecision.SelectedValue = *Compatible;
			Analysis.RowReferenceDecision.Disposition = EDataForgeInferenceDisposition::Exact;
			return;
		}

		Analysis.RowReferenceDecision.SelectedValue = Requested.RowReferenceProperty;
		Analysis.RowReferenceDecision.Disposition = EDataForgeInferenceDisposition::Conflict;
		FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = EDataForgeSeverity::Error;
		Diagnostic.Code = TEXT("DF2050");
		Diagnostic.Message = FString::Printf(TEXT("Explicit Row Reference '%s' is not compatible with output '%s'."),
			*Requested.RowReferenceProperty, *Requested.OutputName.ToString());
		Diagnostic.Field = Requested.OutputName;
	}

	FString SlotSelection(const FDataForgeReflectedSlotCandidate& Slot)
	{
		return FString::Printf(TEXT("%s|%s|%s|%d|%d"),
			*Slot.AssetKind.ToString(), *Slot.Role.ToString(), *Slot.TargetProperty,
			static_cast<uint8>(Slot.Cardinality), static_cast<uint8>(Slot.Reconcile));
	}

	FString AssociationSelection(const FDataForgeAssociationSemanticMapping& Mapping)
	{
		return FString::Printf(TEXT("%s|%s|%s|%s"), *Mapping.MatchColumn.ToString(),
			*Mapping.AssetPathColumn.ToString(), *Mapping.AssetKindColumn.ToString(), *Mapping.RoleColumn.ToString());
	}

	bool PackageExists(const FString& ObjectOrPackagePath)
	{
		if (ObjectOrPackagePath.IsEmpty()) return false;
		FString PackagePath = ObjectOrPackagePath;
		if (ObjectOrPackagePath.Contains(TEXT("."))) PackagePath = FPackageName::ObjectPathToPackageName(ObjectOrPackagePath);
		return FPackageName::IsValidLongPackageName(PackagePath) && FPackageName::DoesPackageExist(PackagePath);
	}
}

FDataForgeAuthoringPlannerResult FDataForgeAuthoringPlanner::BuildPlan(const FDataForgeAuthoringPlannerRequest& Request)
{
	using namespace DataForgeAuthoringPlanner;
	FDataForgeAuthoringPlannerResult Result;
	Result.Plan.Intent = Request.Intent;
	Result.Plan.Intent.Normalize();
	Result.SourceColumns = Request.PrimaryData.Columns;
	Result.SourceColumns.Sort(FNameLexicalLess());
	for (int32 Index = Result.SourceColumns.Num() - 1; Index > 0; --Index)
	{
		if (Result.SourceColumns[Index] == Result.SourceColumns[Index - 1]) Result.SourceColumns.RemoveAt(Index);
	}
	FDataForgeAuthoringDecision SourceColumnsDecision;
	SourceColumnsDecision.DecisionId = TEXT("SourceColumns");
	SourceColumnsDecision.Disposition = EDataForgeInferenceDisposition::Exact;
	SourceColumnsDecision.bRequired = false;
	SourceColumnsDecision.SelectedValue = FString::JoinBy(Result.SourceColumns, TEXT("|"), [](FName Column)
	{
		return Column.ToString();
	});
	Result.Plan.Decisions.Add(MoveTemp(SourceColumnsDecision));

	Result.PrimaryKey = FDataForgeSourceFolderAnalyzer::AnalyzePrimaryKey(
		Request.PrimaryData, Result.Plan.Intent.PreferredPrimaryKey);
	Result.Plan.Decisions.Add(Result.PrimaryKey.Decision);
	AppendDiagnostics(Result.Plan.Diagnostics, Result.PrimaryKey.Diagnostics);
	const FName SourceKey(*Result.PrimaryKey.Decision.SelectedValue);

	for (const FString& Root : Result.Plan.Intent.AssetSearchRoots)
	{
		const FDataForgeObservedAssetRoot* Observed = Request.AssetRoots.FindByPredicate([&Root](const FDataForgeObservedAssetRoot& Candidate)
		{
			return NormalizeFolder(Candidate.RootFolder).Equals(Root, ESearchCase::CaseSensitive);
		});
		if (!Observed || SourceKey.IsNone())
		{
			Result.Plan.Decisions.Add(MakeFolderUnavailableDecision(Root));
			FDataForgeDiagnostic& Diagnostic = Result.Plan.Diagnostics.AddDefaulted_GetRef();
			Diagnostic.Severity = EDataForgeSeverity::Error;
			Diagnostic.Code = Observed ? TEXT("DF2051") : TEXT("DF2052");
			Diagnostic.Message = Observed
				? FString::Printf(TEXT("Folder '%s' cannot be analyzed until a Primary Key is resolved."), *Root)
				: FString::Printf(TEXT("Folder '%s' has no injected registry observations."), *Root);
			continue;
		}

		FDataForgeFolderLayoutAnalysis Layout = FDataForgeSourceFolderAnalyzer::AnalyzeFolderLayout(
			Root, Request.PrimaryData, SourceKey, Observed->Observations);
		Layout.Decision.DecisionId = FName(*FString::Printf(TEXT("FolderLayout.%s"), *Root));
		Result.Plan.Decisions.Add(Layout.Decision);
		AppendDiagnostics(Result.Plan.Diagnostics, Layout.Diagnostics);
		Result.FolderLayouts.Add(MoveTemp(Layout));
	}

	for (const FDataForgeRequestedOutput& Requested : Result.Plan.Intent.Outputs)
	{
		UClass* OutputClass = Requested.AssetClass.Get();
		if (!OutputClass) OutputClass = Requested.AssetClass.LoadSynchronous();
		FDataForgeOutputReflectionAnalysis Output = FDataForgeOutputReflectionAnalyzer::Analyze(
			OutputClass, Requested.OutputName, Result.Plan.Intent.RowStruct.Get(), Result.Plan.Intent.AssignmentHints);
		ApplyExplicitRowReference(Output, Requested, Result.Plan.Diagnostics);
		Result.Plan.Decisions.Add(Output.RowReferenceDecision);
		AppendDiagnostics(Result.Plan.Diagnostics, Output.Diagnostics);

		for (const FDataForgeReflectedSlotCandidate& Slot : Output.Slots)
		{
			FDataForgeAuthoringDecision Decision;
			Decision.DecisionId = FName(*FString::Printf(TEXT("Output.%s.Slot.%s"),
				*Requested.OutputName.ToString(), *Slot.SlotId.ToString()));
			Decision.Disposition = Slot.Disposition;
			Decision.bRequired = Slot.bRequired;
			Decision.SelectedValue = SlotSelection(Slot);
			Decision.Evidence = Slot.Evidence;
			Result.Plan.Decisions.Add(MoveTemp(Decision));
		}
		Result.Outputs.Add(MoveTemp(Output));
	}

	for (const FDataForgeAssociationSchema& Schema : Request.AssociationSchemas)
	{
		const FName AdapterId = Schema.AdapterId.IsNone() ? Schema.Source.AdapterId : Schema.AdapterId;
		FDataForgeAssociationSemanticMapping Mapping =
			FDataForgeOutputReflectionAnalyzer::ResolveAssociationSemantics(AdapterId, Schema.Columns);
		FDataForgeAuthoringDecision Decision;
		Decision.DecisionId = FName(*FString::Printf(TEXT("Association.%s.Columns"), *Schema.SourceId.ToString()));
		Decision.Disposition = Mapping.Disposition;
		Decision.bRequired = true;
		Decision.SelectedValue = AssociationSelection(Mapping);
		Decision.Evidence = Mapping.Evidence;
		Result.Plan.Decisions.Add(MoveTemp(Decision));
		AppendDiagnostics(Result.Plan.Diagnostics, Mapping.Diagnostics);
		FDataForgeResolvedAssociationSchema& Resolved = Result.Associations.AddDefaulted_GetRef();
		Resolved.Schema = Schema;
		Resolved.Mapping = MoveTemp(Mapping);
	}

	if (!Result.Plan.Intent.DataTablePath.IsEmpty())
	{
		FDataForgeAuthoringArtifact& Artifact = Result.Plan.Artifacts.AddDefaulted_GetRef();
		Artifact.ArtifactKind = TEXT("DataTable");
		Artifact.Action = PackageExists(Result.Plan.Intent.DataTablePath)
			? EDataForgeAuthoringArtifactAction::Reuse
			: EDataForgeAuthoringArtifactAction::Create;
		Artifact.ObjectPath = Result.Plan.Intent.DataTablePath;
	}

	Result.Plan.Normalize();
	return Result;
}
