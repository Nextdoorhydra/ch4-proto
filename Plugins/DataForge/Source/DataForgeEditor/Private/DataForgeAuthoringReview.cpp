#include "DataForgeAuthoringReview.h"

#include "DataForgeBindingPreset.h"
#include "DataForgeFolderSource.h"
#include "DataForgeRuleSet.h"
#include "Misc/PackageName.h"

namespace DataForgeAuthoringReview
{
	FString DispositionText(EDataForgeInferenceDisposition Disposition)
	{
		switch (Disposition)
		{
		case EDataForgeInferenceDisposition::Exact: return TEXT("Exact");
		case EDataForgeInferenceDisposition::Recommended: return TEXT("Recommended");
		case EDataForgeInferenceDisposition::Ambiguous: return TEXT("Ambiguous");
		case EDataForgeInferenceDisposition::Unsupported: return TEXT("Unsupported");
		case EDataForgeInferenceDisposition::Conflict: return TEXT("Conflict");
		default: return TEXT("Unresolved");
		}
	}

	FString ArtifactActionText(EDataForgeAuthoringArtifactAction Action)
	{
		return Action == EDataForgeAuthoringArtifactAction::Reuse ? TEXT("Reuse") : TEXT("Create");
	}

	FString CardinalityText(EDataForgeBindingCardinality Cardinality)
	{
		switch (Cardinality)
		{
		case EDataForgeBindingCardinality::Many: return TEXT("Many");
		case EDataForgeBindingCardinality::OptionalOne: return TEXT("Optional One");
		default: return TEXT("One");
		}
	}

	FString BindingSourceText(const FDataForgeBindingRule& Binding)
	{
		if (Binding.Source == EDataForgeBindingSource::GeneratedOutput)
		{
			return FString::Printf(TEXT("Generated Output '%s'"), *Binding.SourceOutput.ToString());
		}
		if (Binding.Source == EDataForgeBindingSource::ResolvedAsset)
		{
			return FString::Printf(TEXT("Resolved Asset '%s' via rule '%s'"),
				*Binding.SourceColumn.ToString(), *Binding.AssetRuleId.ToString());
		}
		return FString::Printf(TEXT("Source Column '%s'"), *Binding.SourceColumn.ToString());
	}

	FString BindingTargetText(const FDataForgeBindingRule& Binding)
	{
		return Binding.Target == EDataForgeBindingTarget::GeneratedOutput
			? FString::Printf(TEXT("Generated Output '%s'.%s"), *Binding.TargetOutput.ToString(), *Binding.TargetProperty)
			: FString::Printf(TEXT("DataTable Row.%s"), *Binding.TargetProperty);
	}

	FString ObjectPath(const FString& PackagePath)
	{
		return PackagePath + TEXT(".") + FPackageName::GetLongPackageAssetName(PackagePath);
	}

	FString StemFromRuleSet(const FString& RuleSetPath)
	{
		FString Stem = FPackageName::GetLongPackageAssetName(RuleSetPath);
		if (Stem.StartsWith(TEXT("RS_"))) Stem.RightChopInline(3);
		return Stem;
	}

	FDataForgeAuthoringReviewSection& AddSection(FDataForgeAuthoringReview& Review, const TCHAR* Title)
	{
		FDataForgeAuthoringReviewSection& Section = Review.Sections.AddDefaulted_GetRef();
		Section.Title = Title;
		return Section;
	}

	void AddEntry(FDataForgeAuthoringReviewSection& Section, FString Label, FString Value)
	{
		FDataForgeAuthoringReviewEntry& Entry = Section.Entries.AddDefaulted_GetRef();
		Entry.Label = MoveTemp(Label);
		Entry.Value = MoveTemp(Value);
	}
}

FString FDataForgeAuthoringReview::ToDisplayString() const
{
	FString Text = TEXT("Automatic Setup Review");
	for (const FDataForgeAuthoringReviewSection& Section : Sections)
	{
		Text += TEXT("\n\n") + Section.Title;
		for (const FDataForgeAuthoringReviewEntry& Entry : Section.Entries)
		{
			Text += FString::Printf(TEXT("\n- %s: %s"), *Entry.Label, *Entry.Value);
			for (const FString& Detail : Entry.Details)
			{
				Text += TEXT("\n    ") + Detail;
			}
		}
	}
	if (!Diagnostics.IsEmpty())
	{
		Text += TEXT("\n\nDiagnostics");
		for (const FDataForgeDiagnostic& Diagnostic : Diagnostics)
		{
			Text += FString::Printf(TEXT("\n- [%s] %s"), *Diagnostic.Code, *Diagnostic.Message);
		}
	}
	return Text;
}

FDataForgeAuthoringReview FDataForgeAuthoringReviewBuilder::Build(
	const FDataForgeAuthoringPlannerResult& Planned,
	const FDataForgeAuthoringDraft& Draft,
	const UDataForgeRuleSet& ConfiguredRuleSet,
	const FString& RuleSetPath,
	const FString& DefinitionFolder,
	const FDataForgeApplyPlan* PreviewPlan)
{
	using namespace DataForgeAuthoringReview;
	FDataForgeAuthoringReview Review;

	FDataForgeAuthoringReviewSection& Decisions = AddSection(Review, TEXT("Inferred Decisions"));
	for (const FDataForgeAuthoringDecision& Decision : Planned.Plan.Decisions)
	{
		FDataForgeAuthoringReviewEntry& Entry = Decisions.Entries.AddDefaulted_GetRef();
		Entry.Label = Decision.DecisionId.ToString();
		Entry.Value = FString::Printf(TEXT("%s - %s"), *DispositionText(Decision.Disposition),
			Decision.SelectedValue.IsEmpty() ? TEXT("(none)") : *Decision.SelectedValue);
		for (const FDataForgeInferenceEvidence& Evidence : Decision.Evidence)
		{
			Entry.Details.Add(FString::Printf(TEXT("Evidence: %s = %s (%s)"),
				*Evidence.Kind.ToString(), *Evidence.Value, *Evidence.Source));
		}
		if (Decision.Alternatives.Num() > 1)
		{
			Entry.Details.Add(TEXT("Alternatives: ") + FString::Join(Decision.Alternatives, TEXT(", ")));
		}
	}

	FDataForgeAuthoringReviewSection& Artifacts = AddSection(Review, TEXT("Files and Definitions"));
	for (const FDataForgeAuthoringArtifact& Artifact : Planned.Plan.Artifacts)
	{
		AddEntry(Artifacts, Artifact.ArtifactKind.ToString(),
			ArtifactActionText(Artifact.Action) + TEXT(" - ") + Artifact.ObjectPath);
	}
	AddEntry(Artifacts, TEXT("RuleSet"), TEXT("Update - ") + RuleSetPath);
	const FString Stem = StemFromRuleSet(RuleSetPath);
	for (int32 Index = 0; Index < Draft.LayoutRecipes.Num(); ++Index)
	{
		const FString Suffix = Index == 0 ? FString() : FString::Printf(TEXT("_%d"), Index + 1);
		AddEntry(Artifacts, TEXT("Asset Layout Recipe"), TEXT("Create - ") + ObjectPath(DefinitionFolder + TEXT("/ALR_") + Stem + Suffix));
	}
	for (int32 Index = 0; Index < Draft.FolderSources.Num(); ++Index)
	{
		const FString Suffix = Index == 0 ? FString() : FString::Printf(TEXT("_%d"), Index + 1);
		AddEntry(Artifacts, TEXT("Folder Source Config"), TEXT("Create - ") + ObjectPath(DefinitionFolder + TEXT("/FSC_") + Stem + Suffix));
	}
	for (int32 Index = 0; Index < Draft.BindingPresets.Num(); ++Index)
	{
		const FString Suffix = Index == 0 ? FString() : FString::Printf(TEXT("_%d"), Index + 1);
		AddEntry(Artifacts, TEXT("Binding Preset"), TEXT("Create - ") + ObjectPath(DefinitionFolder + TEXT("/BP_") + Stem + Suffix));
	}
	for (const FDataForgeAssociationSourceRule& Association : ConfiguredRuleSet.AssociationSources)
	{
		if (!Association.Source.SourceAsset.IsNull())
		{
			AddEntry(Artifacts, TEXT("Association Definition"),
				TEXT("Reuse - ") + Association.Source.SourceAsset.ToSoftObjectPath().ToString());
		}
	}

	FDataForgeAuthoringReviewSection& Relationships = AddSection(Review, TEXT("Assignment Relationships"));
	for (const TStrongObjectPtr<UDataForgeBindingPreset>& Preset : Draft.BindingPresets)
	{
		if (!Preset) continue;
		for (const FDataForgeBindingPresetSlot& Slot : Preset->Slots)
		{
			AddEntry(Relationships,
				FString::Printf(TEXT("%s / %s / %s"), *Preset->OutputName.ToString(), *Slot.AssetKind.ToString(), *Slot.Role.ToString()),
				FString::Printf(TEXT("%s -> %s (%s)"), *Slot.AssociationSourceId.ToString(), *Slot.TargetProperty,
					*CardinalityText(Slot.Cardinality)));
		}
	}
	for (const FDataForgeBindingRule& Binding : ConfiguredRuleSet.Bindings)
	{
		AddEntry(Relationships, BindingSourceText(Binding), BindingTargetText(Binding));
	}

	if (PreviewPlan)
	{
		FDataForgeAuthoringReviewSection& Effects = AddSection(Review, TEXT("Preview Effects"));
		AddEntry(Effects, TEXT("DataTable Rows"), FString::Printf(TEXT("Create %d, Update %d, Unchanged %d, Orphan %d"),
			PreviewPlan->CreateCount, PreviewPlan->UpdateCount, PreviewPlan->UnchangedCount, PreviewPlan->OrphanCount));
		AddEntry(Effects, TEXT("Managed Assets"), FString::Printf(TEXT("Create %d, Move %d, Update %d, Unchanged %d, Orphan %d"),
			PreviewPlan->AssetCreateCount, PreviewPlan->AssetMoveCount, PreviewPlan->AssetUpdateCount,
			PreviewPlan->AssetUnchangedCount, PreviewPlan->AssetOrphanCount));
		Review.Diagnostics.Append(PreviewPlan->Diagnostics);
	}
	Review.Diagnostics.Append(Planned.Plan.Diagnostics);
	Review.Diagnostics.Append(Draft.Diagnostics);
	return Review;
}
