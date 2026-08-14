#include "DataForgeAuthoringMaterializer.h"

#include "DataForgeBindingPreset.h"
#include "DataForgeBindingPresetAuthoring.h"
#include "DataForgeEditorService.h"
#include "DataForgeFolderSource.h"
#include "DataForgeNamingPolicy.h"
#include "DataForgeRuleSet.h"
#include "Misc/PackageName.h"

namespace DataForgeAuthoringMaterializer
{
	void AddDiagnostic(TArray<FDataForgeDiagnostic>& Diagnostics, const TCHAR* Code, const FString& Message, FName Field = NAME_None)
	{
		FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = EDataForgeSeverity::Error;
		Diagnostic.Code = Code;
		Diagnostic.Message = Message;
		Diagnostic.Field = Field;
	}

	bool HasErrors(const TArray<FDataForgeDiagnostic>& Diagnostics)
	{
		return Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
		{
			return Diagnostic.Severity == EDataForgeSeverity::Error;
		});
	}

	void AppendDiagnostics(TArray<FDataForgeDiagnostic>& Target, const TArray<FDataForgeDiagnostic>& Source)
	{
		Target.Append(Source);
	}

	FName EffectiveAdapterId(const FDataForgeAssociationSchema& Schema)
	{
		return Schema.AdapterId.IsNone() ? Schema.Source.AdapterId : Schema.AdapterId;
	}

	const FDataForgeFolderLayoutCandidate* SelectedLayout(const FDataForgeFolderLayoutAnalysis& Layout)
	{
		return Layout.Candidates.FindByPredicate([&Layout](const FDataForgeFolderLayoutCandidate& Candidate)
		{
			return Candidate.Pattern == Layout.Decision.SelectedValue;
		});
	}

	FName FolderSourceId(const FDataForgeAuthoringPlannerResult& Planned)
	{
		for (const FDataForgeResolvedAssociationSchema& Association : Planned.Associations)
		{
			if (EffectiveAdapterId(Association.Schema) == TEXT("AssetRegistryFolder"))
			{
				return Association.Schema.SourceId.IsNone() ? FName(TEXT("FolderAssets")) : Association.Schema.SourceId;
			}
		}
		return TEXT("FolderAssets");
	}

	const FDataForgeAssociationSemanticMapping* FolderMapping(const FDataForgeAuthoringPlannerResult& Planned)
	{
		for (const FDataForgeResolvedAssociationSchema& Association : Planned.Associations)
		{
			if (EffectiveAdapterId(Association.Schema) == TEXT("AssetRegistryFolder")) return &Association.Mapping;
		}
		return nullptr;
	}

	FName ResolvePolicyKind(
		const FDataForgeReflectedSlotCandidate& Slot,
		const UDataForgeNamingPolicy& Policy,
		TArray<FDataForgeDiagnostic>* Diagnostics = nullptr)
	{
		if (Policy.AssetKinds.ContainsByPredicate([&Slot](const FDataForgeAssetKindNamingRule& Rule)
		{
			return Rule.AssetKind == Slot.AssetKind;
		}))
		{
			return Slot.AssetKind;
		}

		UClass* SlotClass = Slot.ExpectedAssetClass.LoadSynchronous();
		TArray<const FDataForgeAssetKindNamingRule*> Compatible;
		for (const FDataForgeAssetKindNamingRule& Rule : Policy.AssetKinds)
		{
			UClass* ExpectedClass = Rule.ExpectedAssetClass.LoadSynchronous();
			if (SlotClass && ExpectedClass
				&& (SlotClass->IsChildOf(ExpectedClass) || ExpectedClass->IsChildOf(SlotClass)))
			{
				Compatible.Add(&Rule);
			}
		}
		if (Compatible.Num() == 1) return Compatible[0]->AssetKind;
		if (Diagnostics)
		{
			AddDiagnostic(*Diagnostics, TEXT("DF2067"), FString::Printf(
				TEXT("Slot '%s' Asset Kind '%s' does not resolve uniquely in the selected Naming Policy."),
				*Slot.SlotId.ToString(), *Slot.AssetKind.ToString()), Slot.SlotId);
		}
		return NAME_None;
	}

	void AddAssociationRule(
		UDataForgeRuleSet& RuleSet,
		FName SourceId,
		const FDataForgeSourceConfig& Source,
		const FDataForgeAssociationSemanticMapping& Mapping)
	{
		FDataForgeAssociationSourceRule& Rule = RuleSet.AssociationSources.AddDefaulted_GetRef();
		Rule.SourceId = SourceId;
		Rule.Source = Source;
		Rule.MatchColumn = Mapping.MatchColumn;
		Rule.AssetPathColumn = Mapping.AssetPathColumn;
		Rule.AssetKindColumn = Mapping.AssetKindColumn;
		Rule.RoleColumn = Mapping.RoleColumn;
	}
}

FDataForgeAuthoringDraft FDataForgeAuthoringMaterializer::BuildDraft(
	const FDataForgeAuthoringMaterializationRequest& Request)
{
	using namespace DataForgeAuthoringMaterializer;
	FDataForgeAuthoringDraft Result;
	if (Request.Planned.Plan.HasBlockingIssues())
	{
		AddDiagnostic(Result.Diagnostics, TEXT("DF2060"), TEXT("Authoring Plan contains unresolved blocking decisions."));
		Result.Summary = TEXT("Draft was not created because the Authoring Plan is blocked.");
		return Result;
	}
	if (Request.Planned.Outputs.Num() > 1)
	{
		AddDiagnostic(Result.Diagnostics, TEXT("DF2061"),
			TEXT("Automatic materialization currently supports one generated output because RuleSet stores one Binding Preset."));
	}
	if (Request.Planned.FolderLayouts.Num() > 1)
	{
		AddDiagnostic(Result.Diagnostics, TEXT("DF2062"),
			TEXT("Automatic materialization requires an explicit slot-to-source choice when more than one asset root is analyzed."));
	}
	UDataForgeFolderSourceConfig* ReusableFolderSource = Request.ReusableFolderSource.Get();
	UDataForgeAssetLayoutRecipe* ReusableRecipe = ReusableFolderSource
		? ReusableFolderSource->LayoutRecipe.LoadSynchronous() : nullptr;
	UDataForgeNamingPolicy* NamingPolicy = Request.NamingPolicy.Get();
	if (!NamingPolicy && ReusableRecipe) NamingPolicy = ReusableRecipe->NamingPolicy.LoadSynchronous();
	if (!Request.Planned.FolderLayouts.IsEmpty())
	{
		if (!NamingPolicy)
		{
			AddDiagnostic(Result.Diagnostics, TEXT("DF2063"),
				TEXT("An analyzed asset root requires a reusable Naming Policy; folder analysis cannot safely invent project naming conventions."));
		}
		else
		{
			const FDataForgeResult PolicyValidation = FDataForgeNamingPolicyResolver::ValidatePolicy(*NamingPolicy);
			if (!PolicyValidation.bSuccess)
			{
				AppendDiagnostics(Result.Diagnostics, PolicyValidation.Diagnostics);
			}
		}
	}
	for (const FDataForgeFolderLayoutAnalysis& Layout : Request.Planned.FolderLayouts)
	{
		const FDataForgeFolderLayoutCandidate* Candidate = SelectedLayout(Layout);
		if (!Candidate)
		{
			AddDiagnostic(Result.Diagnostics, TEXT("DF2064"),
				FString::Printf(TEXT("Folder layout '%s' has no selected materialization candidate."), *Layout.RootFolder));
		}
		else if (ReusableFolderSource)
		{
			FString ReusableRoot = ReusableFolderSource->RootFolder;
			ReusableRoot.ReplaceInline(TEXT("\\"), TEXT("/"));
			while (ReusableRoot.EndsWith(TEXT("/"))) ReusableRoot.LeftChopInline(1);
			if (!ReusableRecipe || ReusableRoot != Layout.RootFolder
				|| ReusableRecipe->SubjectSource != EDataForgeLayoutSubjectSource::FolderSegment
				|| ReusableRecipe->SubjectFolderIndex != Candidate->SubjectFolderIndex
				|| ReusableRecipe->KindFolderIndex != Candidate->KindFolderIndex)
			{
				AddDiagnostic(Result.Diagnostics, TEXT("DF2071"),
					TEXT("Reusable Folder Source layout does not match the inferred root/subject/kind structure."));
			}
		}
	}
	int32 FolderAssociationCount = 0;
	for (const FDataForgeResolvedAssociationSchema& Association : Request.Planned.Associations)
	{
		if (EffectiveAdapterId(Association.Schema) == TEXT("AssetRegistryFolder")) ++FolderAssociationCount;
	}
	if (FolderAssociationCount > 1)
	{
		AddDiagnostic(Result.Diagnostics, TEXT("DF2065"), TEXT("Multiple folder Association schemas require an explicit root mapping."));
	}
	int32 AssociationCount = Request.Planned.FolderLayouts.IsEmpty() ? 0 : 1;
	for (const FDataForgeResolvedAssociationSchema& Association : Request.Planned.Associations)
	{
		if (Association.Schema.SourceId.IsNone())
		{
			AddDiagnostic(Result.Diagnostics, TEXT("DF2068"), TEXT("Every Association Source requires a stable Source Id."));
		}
		if (EffectiveAdapterId(Association.Schema) != TEXT("AssetRegistryFolder")) ++AssociationCount;
	}
	int32 InferredSlotCount = 0;
	for (const FDataForgeOutputReflectionAnalysis& Output : Request.Planned.Outputs)
	{
		for (const FDataForgeReflectedSlotCandidate& Slot : Output.Slots)
		{
			if (Slot.Disposition == EDataForgeInferenceDisposition::Unsupported) continue;
			++InferredSlotCount;
			if (NamingPolicy) ResolvePolicyKind(Slot, *NamingPolicy, &Result.Diagnostics);
		}
	}
	if (InferredSlotCount > 0 && AssociationCount == 0)
	{
		AddDiagnostic(Result.Diagnostics, TEXT("DF2069"), TEXT("Inferred binding slots require an Association Source."));
	}
	if (InferredSlotCount > 0 && AssociationCount > 1)
	{
		AddDiagnostic(Result.Diagnostics, TEXT("DF2066"),
			TEXT("Multiple Association Sources require an explicit source selection for each inferred slot."));
	}
	for (const FDataForgeRequestedOutput& Output : Request.Planned.Plan.Intent.Outputs)
	{
		if (!FPackageName::IsValidLongPackageName(Output.OutputFolder))
		{
			AddDiagnostic(Result.Diagnostics, TEXT("DF2070"),
				FString::Printf(TEXT("Generated Output Folder '%s' is not a valid Content Browser folder."), *Output.OutputFolder), Output.OutputName);
		}
	}
	if (HasErrors(Result.Diagnostics))
	{
		Result.Summary = TEXT("Draft validation failed before any object was created.");
		return Result;
	}

	Result.RuleSet = TStrongObjectPtr<UDataForgeRuleSet>(NewObject<UDataForgeRuleSet>(GetTransientPackage()));
	UDataForgeRuleSet& RuleSet = *Result.RuleSet;
	RuleSet.RuleSetId = FGuid::NewGuid();
	RuleSet.Source = Request.Planned.Plan.Intent.Source;
	RuleSet.Schema.PrimaryKey = FName(*Request.Planned.PrimaryKey.Decision.SelectedValue);
	RuleSet.Schema.RequiredColumns = Request.Planned.SourceColumns;
	RuleSet.Output.RowStruct = Request.Planned.Plan.Intent.RowStruct.Get();
	RuleSet.Output.AssetPath = Request.Planned.Plan.Intent.DataTablePath;

	FName DefaultAssociationSource = NAME_None;
	if (Request.Planned.FolderLayouts.Num() == 1)
	{
		const FDataForgeFolderLayoutAnalysis& Layout = Request.Planned.FolderLayouts[0];
		const FDataForgeFolderLayoutCandidate* Candidate = SelectedLayout(Layout);
		UDataForgeFolderSourceConfig* FolderSource = ReusableFolderSource;
		if (!FolderSource)
		{
			UDataForgeAssetLayoutRecipe* Recipe = NewObject<UDataForgeAssetLayoutRecipe>(GetTransientPackage());
			Recipe->RecipeId = TEXT("AutoInferred");
			Recipe->NamingPolicy = NamingPolicy;
			Recipe->SubjectSource = EDataForgeLayoutSubjectSource::FolderSegment;
			Recipe->SubjectFolderIndex = Candidate->SubjectFolderIndex;
			Recipe->KindFolderIndex = Candidate->KindFolderIndex;
			Recipe->bRequireKindFolderMatch = Candidate->KindFolderIndex != INDEX_NONE;
			Result.LayoutRecipes.Emplace(Recipe);

			FolderSource = NewObject<UDataForgeFolderSourceConfig>(GetTransientPackage());
			FolderSource->RootFolder = Layout.RootFolder;
			FolderSource->LayoutRecipe = Recipe;
			for (const FDataForgeOutputReflectionAnalysis& Output : Request.Planned.Outputs)
			{
				for (const FDataForgeReflectedSlotCandidate& Slot : Output.Slots)
				{
					const FName PolicyKind = ResolvePolicyKind(Slot, *NamingPolicy);
					if (!PolicyKind.IsNone()) FolderSource->AllowedAssetKinds.AddUnique(PolicyKind);
				}
			}
			FolderSource->AllowedAssetKinds.Sort(FNameLexicalLess());
			Result.FolderSources.Emplace(FolderSource);
		}

		DefaultAssociationSource = FolderSourceId(Request.Planned);
		FDataForgeSourceConfig Source;
		Source.AdapterId = TEXT("AssetRegistryFolder");
		Source.SourceAsset = FolderSource;
		FDataForgeAssociationSemanticMapping Canonical;
		Canonical.MatchColumn = TEXT("Subject");
		Canonical.AssetPathColumn = TEXT("ObjectPath");
		Canonical.AssetKindColumn = TEXT("AssetKind");
		Canonical.RoleColumn = TEXT("Role");
		AddAssociationRule(RuleSet, DefaultAssociationSource, Source,
			FolderMapping(Request.Planned) ? *FolderMapping(Request.Planned) : Canonical);
	}

	for (const FDataForgeResolvedAssociationSchema& Association : Request.Planned.Associations)
	{
		if (EffectiveAdapterId(Association.Schema) == TEXT("AssetRegistryFolder")) continue;
		FDataForgeSourceConfig Source = Association.Schema.Source;
		if (Source.AdapterId.IsNone()) Source.AdapterId = Association.Schema.AdapterId;
		AddAssociationRule(RuleSet, Association.Schema.SourceId, Source, Association.Mapping);
	}
	if (RuleSet.AssociationSources.Num() == 1) DefaultAssociationSource = RuleSet.AssociationSources[0].SourceId;

	for (const FDataForgeOutputReflectionAnalysis& Output : Request.Planned.Outputs)
	{
		UDataForgeBindingPreset* Preset = NewObject<UDataForgeBindingPreset>(GetTransientPackage());
		Preset->OutputName = Output.OutputName;
		Preset->TargetClass = Output.TargetClass.Get();
		Preset->AssetNamePrefix = Output.TargetClass.IsValid() && Output.TargetClass.Get()->IsChildOf(UPrimaryDataAsset::StaticClass())
			? TEXT("PDA") : TEXT("DA");
		Preset->RowReferenceProperty = Output.RowReferenceDecision.SelectedValue;
		for (const FDataForgeReflectedSlotCandidate& Candidate : Output.Slots)
		{
			if (Candidate.Disposition == EDataForgeInferenceDisposition::Unsupported) continue;
			FDataForgeBindingPresetSlot& Slot = Preset->Slots.AddDefaulted_GetRef();
			Slot.SlotId = Candidate.SlotId;
			Slot.AssetKind = NamingPolicy ? ResolvePolicyKind(Candidate, *NamingPolicy) : Candidate.AssetKind;
			Slot.Role = Candidate.Role;
			Slot.AssociationSourceId = DefaultAssociationSource;
			Slot.SourceKeyColumn = RuleSet.Schema.PrimaryKey;
			Slot.TargetProperty = Candidate.TargetProperty;
			Slot.ExpectedAssetClass = Candidate.ExpectedAssetClass;
			Slot.Cardinality = Candidate.Cardinality;
			Slot.Reconcile = Candidate.Reconcile;
			Slot.bRequired = Candidate.bRequired;
		}
		Result.BindingPresets.Emplace(Preset);

		const FDataForgeRequestedOutput* Requested = Request.Planned.Plan.Intent.Outputs.FindByPredicate([&Output](const FDataForgeRequestedOutput& Candidate)
		{
			return Candidate.OutputName == Output.OutputName;
		});
		const FDataForgeBindingPresetMaterialization Materialized = FDataForgeBindingPresetAuthoring::Materialize(
			RuleSet, *Preset, Requested ? Requested->OutputFolder : FString());
		if (FDataForgeGeneratedAssetOutputRule* Generated = RuleSet.GeneratedOutputs.FindByPredicate([&Output](const FDataForgeGeneratedAssetOutputRule& Candidate)
		{
			return Candidate.OutputName == Output.OutputName;
		}))
		{
			Generated->bAdoptCompatibleUnownedAsset = true;
		}
		Result.Diagnostics.Append(Materialized.Diagnostics);
		if (!Materialized.bSuccess) break;
	}

	FDataForgeEditorService::AutoMapExactNames(RuleSet, RuleSet.Schema.RequiredColumns);
	Result.bSuccess = !HasErrors(Result.Diagnostics);
	Result.Summary = Result.bSuccess
		? FString::Printf(TEXT("Draft ready: %d output(s), %d association source(s), %d binding(s), folder definitions=%s."),
			RuleSet.GeneratedOutputs.Num(), RuleSet.AssociationSources.Num(), RuleSet.Bindings.Num(),
			ReusableFolderSource ? TEXT("reused") : TEXT("new"))
		: TEXT("Draft materialization failed validation.");
	return Result;
}
