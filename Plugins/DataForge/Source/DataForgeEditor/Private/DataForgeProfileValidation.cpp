#include "DataForgeProfileValidation.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DataForgeAssetLayoutProfile.h"
#include "DataForgeRuleSet.h"
#include "Modules/ModuleManager.h"

namespace
{
	void AddProfileDiagnostic(
		TArray<FDataForgeDiagnostic>& Diagnostics,
		EDataForgeSeverity Severity,
		const TCHAR* Code,
		const FString& Message)
	{
		FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = Severity;
		Diagnostic.Code = Code;
		Diagnostic.Message = Message;
	}

	bool HasProfileErrors(const TArray<FDataForgeDiagnostic>& Diagnostics)
	{
		return Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
		{
			return Diagnostic.Severity == EDataForgeSeverity::Error;
		});
	}
}

FString FDataForgeProfileValidationReport::MakeSummary() const
{
	return FString::Printf(TEXT("Profile Validation | Profiles: %d | Profile-backed RuleSets: %d | Outdated: %d | Diagnostics: %d | %s"),
		ProfileCount, ProfiledRuleSetCount, OutdatedRuleSetCount, Diagnostics.Num(), bSuccess ? TEXT("Valid") : TEXT("Invalid"));
}

FDataForgeProfileValidationReport FDataForgeProfileValidation::ValidateProject()
{
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.SearchAllAssets(true);
	TArray<FAssetData> ProfileAssets;
	TArray<FAssetData> RuleSetAssets;
	Registry.GetAssetsByClass(UDataForgeAssetLayoutProfile::StaticClass()->GetClassPathName(), ProfileAssets, true);
	Registry.GetAssetsByClass(UDataForgeRuleSet::StaticClass()->GetClassPathName(), RuleSetAssets, true);
	TArray<UDataForgeAssetLayoutProfile*> Profiles;
	TArray<const UDataForgeRuleSet*> RuleSets;
	for (const FAssetData& Asset : ProfileAssets)
	{
		if (UDataForgeAssetLayoutProfile* Profile = Cast<UDataForgeAssetLayoutProfile>(Asset.GetAsset())) Profiles.Add(Profile);
	}
	for (const FAssetData& Asset : RuleSetAssets)
	{
		if (const UDataForgeRuleSet* RuleSet = Cast<UDataForgeRuleSet>(Asset.GetAsset())) RuleSets.Add(RuleSet);
	}
	return Validate(Profiles, RuleSets);
}

FDataForgeProfileValidationReport FDataForgeProfileValidation::Validate(
	TConstArrayView<UDataForgeAssetLayoutProfile*> Profiles,
	TConstArrayView<const UDataForgeRuleSet*> RuleSets)
{
	FDataForgeProfileValidationReport Report;
	Report.ProfileCount = Profiles.Num();
	TMap<FGuid, UDataForgeAssetLayoutProfile*> ProfileById;
	for (UDataForgeAssetLayoutProfile* Profile : Profiles)
	{
		if (!Profile) continue;
		const FDataForgeResult Definition = FDataForgeAssetLayoutMaterializer::ValidateDefinition(*Profile);
		for (FDataForgeDiagnostic Diagnostic : Definition.Diagnostics)
		{
			Diagnostic.Message = FString::Printf(TEXT("%s: %s"), *Profile->GetPathName(), *Diagnostic.Message);
			Report.Diagnostics.Add(MoveTemp(Diagnostic));
		}
		if (UDataForgeAssetLayoutProfile* const* Existing = ProfileById.Find(Profile->ProfileId); Existing && *Existing != Profile)
		{
			AddProfileDiagnostic(Report.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1640"),
				FString::Printf(TEXT("Duplicate ProfileId %s is used by %s and %s."), *Profile->ProfileId.ToString(), *(*Existing)->GetPathName(), *Profile->GetPathName()));
		}
		else if (Profile->ProfileId.IsValid())
		{
			ProfileById.Add(Profile->ProfileId, Profile);
		}
	}

	for (const UDataForgeRuleSet* RuleSet : RuleSets)
	{
		if (!RuleSet || !RuleSet->ProfileOrigin.IsSet()) continue;
		++Report.ProfiledRuleSetCount;
		UDataForgeAssetLayoutProfile* Profile = RuleSet->ProfileOrigin.Profile.LoadSynchronous();
		if (!Profile || Profile->ProfileId != RuleSet->ProfileOrigin.ProfileId)
		{
			AddProfileDiagnostic(Report.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1641"),
				FString::Printf(TEXT("RuleSet %s has missing or mismatched Profile provenance."), *RuleSet->GetPathName()));
			continue;
		}
		const FDataForgeAssetLayoutMaterialization Materialized = FDataForgeAssetLayoutMaterializer::Materialize(
			*Profile, RuleSet->ProfileOrigin.ParameterValues, nullptr);
		if (!Materialized.bSuccess)
		{
			AddProfileDiagnostic(Report.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1642"),
				FString::Printf(TEXT("RuleSet %s cannot materialize its Profile parameters."), *RuleSet->GetPathName()));
			for (FDataForgeDiagnostic Diagnostic : Materialized.Diagnostics)
			{
				Diagnostic.Message = FString::Printf(TEXT("%s: %s"), *RuleSet->GetPathName(), *Diagnostic.Message);
				Report.Diagnostics.Add(MoveTemp(Diagnostic));
			}
			continue;
		}
		if (Materialized.Origin.MaterializedHash != RuleSet->ProfileOrigin.MaterializedHash)
		{
			++Report.OutdatedRuleSetCount;
			AddProfileDiagnostic(Report.Diagnostics, EDataForgeSeverity::Warning, TEXT("DF1643"),
				FString::Printf(TEXT("ProfileOutdated: RuleSet %s was materialized from an older Profile revision."), *RuleSet->GetPathName()));
		}
	}
	Report.bSuccess = !HasProfileErrors(Report.Diagnostics);
	return Report;
}
