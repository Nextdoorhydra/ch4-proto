#include "DataForgePipeline.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DataForgeAssetLayoutProfile.h"
#include "DataForgeBindingPreset.h"
#include "DataForgeFolderSource.h"
#include "DataForgeNamingPolicy.h"
#include "DataForgeRuleSet.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "UObject/MetaData.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/Package.h"

namespace DataForgeFolderSourceAdapter
{
	const TArray<FName> Columns = {
		TEXT("ObjectPath"), TEXT("PackagePath"), TEXT("AssetName"), TEXT("AssetClass"),
		TEXT("AssetKind"), TEXT("TypePrefix"), TEXT("ProjectPrefix"), TEXT("Domain"),
		TEXT("Subject"), TEXT("Role"), TEXT("Variant"), TEXT("Numbering"), TEXT("ConventionStatus")
	};

	void AddDiagnostic(TArray<FDataForgeDiagnostic>& Diagnostics, EDataForgeSeverity Severity, const TCHAR* Code, const FString& Message, int32 SourceRow = INDEX_NONE)
	{
		FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = Severity;
		Diagnostic.Code = Code;
		Diagnostic.Message = Message;
		Diagnostic.SourceRow = SourceRow;
	}

	FString NormalizeFolder(FString Folder)
	{
		Folder.ReplaceInline(TEXT("\\"), TEXT("/"));
		while (Folder.EndsWith(TEXT("/"))) Folder.LeftChopInline(1);
		return Folder;
	}

	bool IsInFolder(const FString& PackagePath, const FString& Folder)
	{
		return PackagePath == Folder || PackagePath.StartsWith(Folder + TEXT("/"), ESearchCase::CaseSensitive);
	}

	bool IsExcludedPath(const UDataForgeFolderSourceConfig& Config, const FString& PackagePath)
	{
		for (const FString& ConfiguredFolder : Config.ExcludedFolders)
		{
			const FString Folder = NormalizeFolder(ConfiguredFolder);
			if (!Folder.IsEmpty() && IsInFolder(PackagePath, Folder)) return true;
		}
		return false;
	}

	bool IsDataForgeDefinitionClass(const UClass* AssetClass)
	{
		return AssetClass && (
			AssetClass->IsChildOf(UDataForgeRuleSet::StaticClass())
			|| AssetClass->IsChildOf(UDataForgeAssetLayoutProfile::StaticClass())
			|| AssetClass->IsChildOf(UDataForgeBindingPreset::StaticClass())
			|| AssetClass->IsChildOf(UDataForgeNamingPolicy::StaticClass())
			|| AssetClass->IsChildOf(UDataForgeAssetLayoutRecipe::StaticClass())
			|| AssetClass->IsChildOf(UDataForgeFolderSourceConfig::StaticClass())
			|| AssetClass->IsChildOf(UDataTable::StaticClass()));
	}

	bool IsManagedAsset(const FAssetData& AssetData, UClass* AssetClass)
	{
		FString ManagedTag;
		if (AssetData.GetTagValue(TEXT("DataForge.Managed"), ManagedTag) && ManagedTag == TEXT("true")) return true;
		if (!AssetClass || !AssetClass->IsChildOf(UDataAsset::StaticClass())) return false;
		if (UObject* Asset = AssetData.GetAsset())
		{
			return Asset->GetOutermost()->GetMetaData().GetValue(Asset, TEXT("DataForge.Managed")) == TEXT("true");
		}
		return false;
	}

	TArray<FString> RelativeSegments(const FString& RootFolder, const FString& PackagePath)
	{
		FString Relative;
		if (PackagePath != RootFolder && PackagePath.StartsWith(RootFolder + TEXT("/"), ESearchCase::CaseSensitive))
		{
			Relative = PackagePath.Mid(RootFolder.Len() + 1);
		}
		TArray<FString> Segments;
		Relative.ParseIntoArray(Segments, TEXT("/"), true);
		return Segments;
	}

	bool ResolveSubject(const UDataForgeAssetLayoutRecipe& Recipe, const TArray<FString>& Segments, FString& OutSubject)
	{
		switch (Recipe.SubjectSource)
		{
		case EDataForgeLayoutSubjectSource::AssetName:
			OutSubject.Reset();
			return true;
		case EDataForgeLayoutSubjectSource::Fixed:
			OutSubject = Recipe.FixedSubject;
			return !OutSubject.IsEmpty();
		case EDataForgeLayoutSubjectSource::FolderSegment:
			if (!Segments.IsValidIndex(Recipe.SubjectFolderIndex)) return false;
			OutSubject = Segments[Recipe.SubjectFolderIndex];
			return !OutSubject.IsEmpty();
		default:
			return false;
		}
	}

	const FDataForgeAssetKindNamingRule* FindKind(const UDataForgeNamingPolicy& Policy, FName AssetKind)
	{
		return Policy.AssetKinds.FindByPredicate([AssetKind](const FDataForgeAssetKindNamingRule& Rule)
		{
			return Rule.AssetKind == AssetKind;
		});
	}

	FString HashInventory(const TArray<FString>& CanonicalRows)
	{
		FString Canonical;
		for (const FString& Row : CanonicalRows)
		{
			Canonical += FString::Printf(TEXT("%d:%s|"), Row.Len(), *Row);
		}
		FTCHARToUTF8 Utf8(*Canonical);
		FMD5 Md5;
		Md5.Update(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		uint8 Digest[16];
		Md5.Final(Digest);
		return BytesToHex(Digest, UE_ARRAY_COUNT(Digest));
	}
}

FDataForgeSourceDescriptor FDataForgeAssetRegistryFolderSourceAdapter::Describe() const
{
	return {
		TEXT("AssetRegistryFolder"),
		NSLOCTEXT("DataForge", "AssetRegistryFolderAdapter", "Asset Registry Folder"),
		TEXT("Reads a Content Browser folder into normalized asset inventory rows using a Layout Recipe and Naming Policy."),
		FString()
	};
}

bool FDataForgeAssetRegistryFolderSourceAdapter::Probe(
	const FDataForgeSourceConfig& Source,
	FDataForgeDataSet& OutDataSet,
	TArray<FDataForgeDiagnostic>& OutDiagnostics) const
{
	return Read(Source, FMath::Max(1, Source.ProbeRowLimit), OutDataSet, OutDiagnostics);
}

bool FDataForgeAssetRegistryFolderSourceAdapter::Fetch(
	const FDataForgeSourceConfig& Source,
	FDataForgeDataSet& OutDataSet,
	TArray<FDataForgeDiagnostic>& OutDiagnostics) const
{
	return Read(Source, INDEX_NONE, OutDataSet, OutDiagnostics);
}

bool FDataForgeAssetRegistryFolderSourceAdapter::Read(
	const FDataForgeSourceConfig& Source,
	int32 RowLimit,
	FDataForgeDataSet& OutDataSet,
	TArray<FDataForgeDiagnostic>& OutDiagnostics)
{
	OutDataSet = {};
	OutDataSet.Columns = DataForgeFolderSourceAdapter::Columns;
	UDataForgeFolderSourceConfig* Config = Cast<UDataForgeFolderSourceConfig>(Source.SourceAsset.LoadSynchronous());
	if (!Config)
	{
		DataForgeFolderSourceAdapter::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1801"), TEXT("Asset Registry Folder requires a Folder Source Config asset."));
		return false;
	}
	const FString RootFolder = DataForgeFolderSourceAdapter::NormalizeFolder(Config->RootFolder);
	if (!FPackageName::IsValidLongPackageName(RootFolder))
	{
		DataForgeFolderSourceAdapter::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1802"), FString::Printf(TEXT("Folder Source Root '%s' is not a valid Content Browser path."), *RootFolder));
		return false;
	}
	UDataForgeAssetLayoutRecipe* Recipe = Config->LayoutRecipe.LoadSynchronous();
	if (!Recipe)
	{
		DataForgeFolderSourceAdapter::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1803"), TEXT("Folder Source Config requires an Asset Layout Recipe."));
		return false;
	}
	UDataForgeNamingPolicy* NamingPolicy = Recipe->NamingPolicy.LoadSynchronous();
	if (!NamingPolicy || !FDataForgeNamingPolicyResolver::ValidatePolicy(*NamingPolicy).bSuccess)
	{
		DataForgeFolderSourceAdapter::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1804"), TEXT("Asset Layout Recipe requires a valid Naming Policy."));
		return false;
	}
	if (Recipe->SubjectSource == EDataForgeLayoutSubjectSource::Fixed && Recipe->FixedSubject.IsEmpty())
	{
		DataForgeFolderSourceAdapter::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Error, TEXT("DF1805"), TEXT("Fixed-subject Layout Recipe requires a Subject value."));
		return false;
	}

	TArray<FAssetData> Assets;
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.GetAssetsByPath(FName(*RootFolder), Assets, Config->bRecursive, false);
	Assets.Sort([](const FAssetData& Left, const FAssetData& Right)
	{
		return Left.GetSoftObjectPath().ToString() < Right.GetSoftObjectPath().ToString();
	});

	TArray<FString> CanonicalRows;
	int32 InventoryIndex = 0;
	for (const FAssetData& AssetData : Assets)
	{
		const FString PackagePath = AssetData.PackagePath.ToString();
		if (DataForgeFolderSourceAdapter::IsExcludedPath(*Config, PackagePath) || AssetData.IsRedirector()) continue;
		UClass* AssetClass = AssetData.GetClass(EResolveClass::Yes);
		if (DataForgeFolderSourceAdapter::IsDataForgeDefinitionClass(AssetClass)) continue;
		if (Config->bExcludeDataForgeManagedAssets && DataForgeFolderSourceAdapter::IsManagedAsset(AssetData, AssetClass)) continue;

		const TArray<FString> Segments = DataForgeFolderSourceAdapter::RelativeSegments(RootFolder, PackagePath);
		FString Subject;
		const bool bSubjectResolved = DataForgeFolderSourceAdapter::ResolveSubject(*Recipe, Segments, Subject);
		FDataForgeNamingParseContext ParseContext;
		ParseContext.Subject = Subject;
		ParseContext.ActualAssetClass = AssetClass;
		const FString AssetName = AssetData.AssetName.ToString();
		const FDataForgeNamingResult Naming = FDataForgeNamingPolicyResolver::Parse(*NamingPolicy, AssetName, ParseContext);
		const FName AssetKind = Naming.Identity.AssetKind;
		if (!Config->AllowedAssetKinds.IsEmpty() && !Config->AllowedAssetKinds.Contains(AssetKind)) continue;

		bool bConventionValid = Naming.bSuccess && bSubjectResolved;
		if (!bSubjectResolved)
		{
			DataForgeFolderSourceAdapter::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Warning, TEXT("DF1806"), FString::Printf(TEXT("%s does not contain the Subject folder required by Recipe '%s'."), *AssetData.GetSoftObjectPath().ToString(), *Recipe->RecipeId.ToString()), InventoryIndex);
		}
		for (const FDataForgeDiagnostic& NamingDiagnostic : Naming.Diagnostics)
		{
			DataForgeFolderSourceAdapter::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Warning, TEXT("DF1807"), FString::Printf(TEXT("%s: %s"), *AssetData.GetSoftObjectPath().ToString(), *NamingDiagnostic.Message), InventoryIndex);
		}

		if (Recipe->bRequireKindFolderMatch && Recipe->KindFolderIndex != INDEX_NONE && Naming.bSuccess)
		{
			const FDataForgeAssetKindNamingRule* Kind = DataForgeFolderSourceAdapter::FindKind(*NamingPolicy, AssetKind);
			if (!Kind || !Segments.IsValidIndex(Recipe->KindFolderIndex) || Segments[Recipe->KindFolderIndex] != Kind->FolderName)
			{
				bConventionValid = false;
				DataForgeFolderSourceAdapter::AddDiagnostic(OutDiagnostics, EDataForgeSeverity::Warning, TEXT("DF1808"), FString::Printf(TEXT("%s is not under the expected '%s' Asset Kind folder."), *AssetData.GetSoftObjectPath().ToString(), Kind ? *Kind->FolderName : TEXT("<unknown>")), InventoryIndex);
			}
		}

		const FString ObjectPath = AssetData.GetSoftObjectPath().ToString();
		CanonicalRows.Add(ObjectPath + TEXT("|") + AssetData.AssetClassPath.ToString() + TEXT("|") + FString::FromInt(static_cast<int32>(AssetData.PackageFlags)));
		if (RowLimit == INDEX_NONE || OutDataSet.Rows.Num() < RowLimit)
		{
			FDataForgeRow& Row = OutDataSet.Rows.AddDefaulted_GetRef();
			Row.SourceRow = InventoryIndex;
			Row.Values.Add(TEXT("ObjectPath"), ObjectPath);
			Row.Values.Add(TEXT("PackagePath"), PackagePath);
			Row.Values.Add(TEXT("AssetName"), AssetName);
			Row.Values.Add(TEXT("AssetClass"), AssetData.AssetClassPath.ToString());
			Row.Values.Add(TEXT("AssetKind"), AssetKind.ToString());
			Row.Values.Add(TEXT("TypePrefix"), Naming.Identity.TypePrefix);
			Row.Values.Add(TEXT("ProjectPrefix"), Naming.Identity.ProjectPrefix);
			Row.Values.Add(TEXT("Domain"), Recipe->Domain.ToString());
			Row.Values.Add(TEXT("Subject"), bSubjectResolved ? Subject : FString());
			Row.Values.Add(TEXT("Role"), Naming.Identity.Role);
			Row.Values.Add(TEXT("Variant"), Naming.Identity.Variant);
			Row.Values.Add(TEXT("Numbering"), Naming.Identity.Numbering == INDEX_NONE ? FString() : FString::FromInt(Naming.Identity.Numbering));
			Row.Values.Add(TEXT("ConventionStatus"), bConventionValid ? TEXT("Valid") : TEXT("Invalid"));
		}
		++InventoryIndex;
	}

	OutDataSet.SourceRevision = DataForgeFolderSourceAdapter::HashInventory(CanonicalRows);
	return true;
}
