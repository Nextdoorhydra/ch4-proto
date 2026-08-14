#include "DataForgeAuthoringApply.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DataForgeBindingPreset.h"
#include "DataForgeEditorService.h"
#include "DataForgeFolderSource.h"
#include "DataForgeNamingPolicy.h"
#include "DataForgeRuleSet.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace DataForgeAuthoringApply
{
	struct FPromotionTarget
	{
		UObject* Source = nullptr;
		FString PackagePath;
		FString ObjectPath;
	};

	void AddDiagnostic(TArray<FDataForgeDiagnostic>& Diagnostics, const TCHAR* Code, const FString& Message, FName Field = NAME_None)
	{
		FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = EDataForgeSeverity::Error;
		Diagnostic.Code = Code;
		Diagnostic.Message = Message;
		Diagnostic.Field = Field;
	}

	FString NormalizePath(FString Path)
	{
		Path.TrimStartAndEndInline();
		Path.ReplaceInline(TEXT("\\"), TEXT("/"));
		while (Path.EndsWith(TEXT("/"))) Path.LeftChopInline(1);
		return Path;
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

	void AddTarget(TArray<FPromotionTarget>& Targets, UObject* Source, const FString& PackagePath)
	{
		FPromotionTarget& Target = Targets.AddDefaulted_GetRef();
		Target.Source = Source;
		Target.PackagePath = PackagePath;
		Target.ObjectPath = ObjectPath(PackagePath);
	}

	bool IsOccupied(const FPromotionTarget& Target)
	{
		return FindObject<UObject>(nullptr, *Target.ObjectPath) != nullptr
			|| FPackageName::DoesPackageExist(Target.PackagePath);
	}

	void RollbackObjects(const TArray<UObject*>& Assets)
	{
		for (int32 Index = Assets.Num() - 1; Index >= 0; --Index)
		{
			UObject* Asset = Assets[Index];
			if (!Asset) continue;
			Asset->ClearFlags(RF_Public | RF_Standalone);
			Asset->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
			Asset->MarkAsGarbage();
		}
	}

	bool SaveStaged(
		const TArray<UObject*>& Assets,
		const TArray<FPromotionTarget>& Targets,
		TArray<FDataForgeDiagnostic>& Diagnostics)
	{
		const FString StagingRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DataForge"), TEXT("AuthoringApply"), FGuid::NewGuid().ToString(EGuidFormats::Digits));
		IFileManager& Files = IFileManager::Get();
		Files.MakeDirectory(*StagingRoot, true);
		TArray<FString> StagedFiles;
		TArray<FString> FinalFiles;
		for (int32 Index = 0; Index < Assets.Num(); ++Index)
		{
			UObject* Asset = Assets[Index];
			const FString Staged = FPaths::Combine(StagingRoot, FString::Printf(TEXT("%03d_%s.uasset"), Index, *Asset->GetName()));
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			SaveArgs.SaveFlags = SAVE_NoError;
			if (!UPackage::SavePackage(Asset->GetPackage(), Asset, *Staged, SaveArgs))
			{
				AddDiagnostic(Diagnostics, TEXT("DF2084"), FString::Printf(TEXT("Could not stage asset '%s' for saving."), *Targets[Index].ObjectPath));
				Files.DeleteDirectory(*StagingRoot, false, true);
				return false;
			}
			StagedFiles.Add(Staged);
			FinalFiles.Add(FPackageName::LongPackageNameToFilename(Targets[Index].PackagePath, FPackageName::GetAssetPackageExtension()));
		}

		TArray<FString> MovedFiles;
		for (int32 Index = 0; Index < StagedFiles.Num(); ++Index)
		{
			Files.MakeDirectory(*FPaths::GetPath(FinalFiles[Index]), true);
			if (!Files.Move(*FinalFiles[Index], *StagedFiles[Index], true, true))
			{
				for (const FString& Moved : MovedFiles) Files.Delete(*Moved, false, true, true);
				AddDiagnostic(Diagnostics, TEXT("DF2085"), FString::Printf(TEXT("Could not commit staged asset '%s'."), *Targets[Index].ObjectPath));
				Files.DeleteDirectory(*StagingRoot, false, true);
				return false;
			}
			MovedFiles.Add(FinalFiles[Index]);
		}
		Files.DeleteDirectory(*StagingRoot, false, true);
		return true;
	}
}

FDataForgeAuthoringApplyResult FDataForgeAuthoringApply::Apply(const FDataForgeAuthoringApplyRequest& Request)
{
	using namespace DataForgeAuthoringApply;
	FDataForgeAuthoringApplyResult Result;
	if (!Request.Draft || !Request.Draft->bSuccess || !Request.Draft->RuleSet)
	{
		AddDiagnostic(Result.Diagnostics, TEXT("DF2080"), TEXT("Apply requires a successful reviewed Authoring Draft."));
		Result.Summary = TEXT("No assets were created.");
		return Result;
	}

	const FDataForgeResult Preview = FDataForgeEditorService::Preview(*Request.Draft->RuleSet);
	Result.Diagnostics.Append(Preview.Diagnostics);
	if (!Preview.bSuccess)
	{
		AddDiagnostic(Result.Diagnostics, TEXT("DF2081"), TEXT("Draft Preview failed; persistent assets were not created."));
		Result.Summary = Preview.Summary;
		return Result;
	}

	const FString RuleSetPath = NormalizePath(Request.RuleSetPath);
	const FString Definitions = Request.DefinitionFolder.IsEmpty()
		? FPackageName::GetLongPackagePath(RuleSetPath) + TEXT("/Definitions")
		: NormalizePath(Request.DefinitionFolder);
	if (!RuleSetPath.StartsWith(TEXT("/Game/")) || !FPackageName::IsValidLongPackageName(RuleSetPath)
		|| !Definitions.StartsWith(TEXT("/Game/")) || !FPackageName::IsValidLongPackageName(Definitions))
	{
		AddDiagnostic(Result.Diagnostics, TEXT("DF2082"), TEXT("RuleSet and Definition paths must be valid /Game package paths."));
		Result.Summary = TEXT("No assets were created.");
		return Result;
	}
	if (Request.ExistingRuleSet && Request.bSaveAssets)
	{
		AddDiagnostic(Result.Diagnostics, TEXT("DF2088"),
			TEXT("Existing RuleSet promotion must remain unsaved so the editor transaction and Save All workflow can own the update."));
		Result.Summary = TEXT("No assets were created.");
		return Result;
	}
	if (Request.ExistingRuleSet && Request.ExistingRuleSet->GetPathName() != ObjectPath(RuleSetPath))
	{
		AddDiagnostic(Result.Diagnostics, TEXT("DF2089"), TEXT("Existing RuleSet does not match the requested RuleSet path."));
		Result.Summary = TEXT("No assets were created.");
		return Result;
	}
	for (const TStrongObjectPtr<UDataForgeAssetLayoutRecipe>& Recipe : Request.Draft->LayoutRecipes)
	{
		if (!Recipe || !Recipe->NamingPolicy.IsValid() || Recipe->NamingPolicy.Get()->GetPackage() == GetTransientPackage())
		{
			AddDiagnostic(Result.Diagnostics, TEXT("DF2086"), TEXT("Apply requires a saved reusable Naming Policy."));
			Result.Summary = TEXT("No assets were created.");
			return Result;
		}
	}

	const FString Stem = StemFromRuleSet(RuleSetPath);
	TArray<FPromotionTarget> Targets;
	for (int32 Index = 0; Index < Request.Draft->LayoutRecipes.Num(); ++Index)
	{
		const FString Suffix = Index == 0 ? FString() : FString::Printf(TEXT("_%d"), Index + 1);
		AddTarget(Targets, Request.Draft->LayoutRecipes[Index].Get(), Definitions + TEXT("/ALR_") + Stem + Suffix);
	}
	for (int32 Index = 0; Index < Request.Draft->FolderSources.Num(); ++Index)
	{
		const FString Suffix = Index == 0 ? FString() : FString::Printf(TEXT("_%d"), Index + 1);
		AddTarget(Targets, Request.Draft->FolderSources[Index].Get(), Definitions + TEXT("/FSC_") + Stem + Suffix);
	}
	for (int32 Index = 0; Index < Request.Draft->BindingPresets.Num(); ++Index)
	{
		const FString Suffix = Index == 0 ? FString() : FString::Printf(TEXT("_%d"), Index + 1);
		AddTarget(Targets, Request.Draft->BindingPresets[Index].Get(), Definitions + TEXT("/BP_") + Stem + Suffix);
	}
	AddTarget(Targets, Request.Draft->RuleSet.Get(), RuleSetPath);

	TSet<FString> UniquePaths;
	for (const FPromotionTarget& Target : Targets)
	{
		const bool bAllowedExistingRuleSet = Request.ExistingRuleSet
			&& Target.Source == Request.Draft->RuleSet.Get()
			&& Target.ObjectPath == Request.ExistingRuleSet->GetPathName();
		if (!Target.Source || !FPackageName::IsValidLongPackageName(Target.PackagePath)
			|| UniquePaths.Contains(Target.PackagePath) || (!bAllowedExistingRuleSet && IsOccupied(Target)))
		{
			AddDiagnostic(Result.Diagnostics, TEXT("DF2083"), FString::Printf(TEXT("Apply target is invalid, duplicated, or already exists: %s"), *Target.ObjectPath));
		}
		UniquePaths.Add(Target.PackagePath);
	}
	if (Result.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic) { return Diagnostic.Severity == EDataForgeSeverity::Error; }))
	{
		Result.Summary = TEXT("No assets were created because one or more target paths are unavailable.");
		return Result;
	}

	TArray<UObject*> PromotedAssets;
	TArray<UObject*> Created;
	TMap<const UObject*, UObject*> PromotedBySource;
	for (const FPromotionTarget& Target : Targets)
	{
		if (Request.ExistingRuleSet && Target.Source == Request.Draft->RuleSet.Get())
		{
			PromotedAssets.Add(Request.ExistingRuleSet);
			PromotedBySource.Add(Target.Source, Request.ExistingRuleSet);
			continue;
		}
		UPackage* Package = CreatePackage(*Target.PackagePath);
		const FName AssetName(*FPackageName::GetLongPackageAssetName(Target.PackagePath));
		UObject* Asset = DuplicateObject(Target.Source, Package, AssetName);
		if (!Asset)
		{
			AddDiagnostic(Result.Diagnostics, TEXT("DF2087"), FString::Printf(TEXT("Could not duplicate draft object to '%s'."), *Target.ObjectPath));
			RollbackObjects(Created);
			Result.Summary = TEXT("Apply failed and created objects were rolled back.");
			return Result;
		}
		Asset->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
		PromotedAssets.Add(Asset);
		Created.Add(Asset);
		PromotedBySource.Add(Target.Source, Asset);
	}

	for (const TStrongObjectPtr<UDataForgeFolderSourceConfig>& DraftSource : Request.Draft->FolderSources)
	{
		UDataForgeFolderSourceConfig* Source = CastChecked<UDataForgeFolderSourceConfig>(PromotedBySource.FindChecked(DraftSource.Get()));
		if (UObject* const* Recipe = PromotedBySource.Find(DraftSource->LayoutRecipe.Get())) Source->LayoutRecipe = CastChecked<UDataForgeAssetLayoutRecipe>(*Recipe);
	}
	UDataForgeRuleSet* RuleSet = CastChecked<UDataForgeRuleSet>(PromotedBySource.FindChecked(Request.Draft->RuleSet.Get()));
	if (Request.ExistingRuleSet)
	{
		const UDataForgeRuleSet& DraftRuleSet = *Request.Draft->RuleSet;
		RuleSet->Modify();
		RuleSet->RuleVersion = DraftRuleSet.RuleVersion;
		RuleSet->Source = DraftRuleSet.Source;
		RuleSet->Schema = DraftRuleSet.Schema;
		RuleSet->Output = DraftRuleSet.Output;
		RuleSet->AssetRules = DraftRuleSet.AssetRules;
		RuleSet->ProfileOrigin = DraftRuleSet.ProfileOrigin;
		RuleSet->BindingPreset = DraftRuleSet.BindingPreset;
		RuleSet->AssociationSources = DraftRuleSet.AssociationSources;
		RuleSet->GeneratedOutputs = DraftRuleSet.GeneratedOutputs;
		RuleSet->Bindings = DraftRuleSet.Bindings;
		RuleSet->Dependencies = DraftRuleSet.Dependencies;
	}
	if (Request.Draft->BindingPresets.Num() == 1
		&& Request.Draft->RuleSet->BindingPreset.Get() == Request.Draft->BindingPresets[0].Get())
	{
		RuleSet->BindingPreset = CastChecked<UDataForgeBindingPreset>(PromotedBySource.FindChecked(Request.Draft->BindingPresets[0].Get()));
	}
	for (FDataForgeAssociationSourceRule& Association : RuleSet->AssociationSources)
	{
		if (UObject* const* Source = PromotedBySource.Find(Association.Source.SourceAsset.Get())) Association.Source.SourceAsset = *Source;
	}

	if (Request.bSaveAssets && !SaveStaged(PromotedAssets, Targets, Result.Diagnostics))
	{
		RollbackObjects(Created);
		Result.Summary = TEXT("Apply failed and staged files were rolled back.");
		return Result;
	}

	for (UObject* Asset : Created)
	{
		FAssetRegistryModule::AssetCreated(Asset);
		Asset->GetPackage()->SetDirtyFlag(!Request.bSaveAssets);
		Result.CreatedAssets.Add(Asset);
		Result.CreatedObjectPaths.Add(Asset->GetPathName());
	}
	if (Request.ExistingRuleSet) RuleSet->MarkPackageDirty();
	Result.RuleSet = RuleSet;
	Result.bSuccess = true;
	Result.Summary = FString::Printf(TEXT("Created %d DataForge definition asset(s); RuleSet=%s; saved=%s."),
		Created.Num(), *RuleSet->GetPathName(), Request.bSaveAssets ? TEXT("true") : TEXT("false"));
	return Result;
}
