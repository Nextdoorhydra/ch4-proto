#include "NKMLocalizationPathConfigurator.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Internationalization/Internationalization.h"
#include "NKMLocalizationSettings.h"

namespace
{
	bool SaveTextAtomically(const FString& Filename, const FString& Text)
	{
		const FString TemporaryPath = Filename + TEXT(".tmp");
		if (!FFileHelper::SaveStringToFile(Text, *TemporaryPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			return false;
		}
		if (!IFileManager::Get().Move(*Filename, *TemporaryPath, true, true, false, true))
		{
			IFileManager::Get().Delete(*TemporaryPath, false, true);
			return false;
		}
		return true;
	}

	bool ReplaceManagedRoot(
		const FString& Filename,
		const FString& PreviousVirtualRoot,
		const FString& CurrentVirtualRoot,
		const FString& PreviousTargetRoot,
		const FString& CurrentTargetRoot,
		FNKMLocalizationResult& OutResult)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Filename))
		{
			OutResult.AddError(TEXT("NKMLOC_PATH_CONFIG_READ"), FString::Printf(TEXT("Unable to read path config '%s'."), *Filename));
			return false;
		}
		const FString OriginalText = Text;

		const FString PreviousContentRoot = FNKMLocalizationPathConfigurator::ToProjectContentPath(PreviousVirtualRoot);
		const FString CurrentContentRoot = FNKMLocalizationPathConfigurator::ToProjectContentPath(CurrentVirtualRoot);
		Text.ReplaceInline(*PreviousVirtualRoot, *CurrentVirtualRoot, ESearchCase::CaseSensitive);
		Text.ReplaceInline(*PreviousContentRoot, *CurrentContentRoot, ESearchCase::CaseSensitive);

		// Migrate the original Phase 1-3 default even when no applied-root state existed yet.
		Text.ReplaceInline(TEXT("/Game/NKM/Localization/StringTables"), *CurrentVirtualRoot, ESearchCase::CaseSensitive);
		Text.ReplaceInline(TEXT("Content/NKM/Localization/StringTables"), *CurrentContentRoot, ESearchCase::CaseSensitive);
		Text.ReplaceInline(TEXT("Content/NetKarma//StringTablesLocalization/StringTables"), *CurrentContentRoot, ESearchCase::CaseSensitive);
		Text.ReplaceInline(*PreviousTargetRoot, *CurrentTargetRoot, ESearchCase::CaseSensitive);
		Text.ReplaceInline(TEXT("Content/NetKarma/Localization/NKMText"), *CurrentTargetRoot, ESearchCase::CaseSensitive);
		Text.ReplaceInline(TEXT("Content/Localization/NKMText"), *CurrentTargetRoot, ESearchCase::CaseSensitive);

		const FString LocalizationPath = TEXT("+LocalizationPaths=%GAMEDIR%") + CurrentTargetRoot;
		Text.ReplaceInline(*(LocalizationPath + TEXT("\r\n") + LocalizationPath), *LocalizationPath, ESearchCase::CaseSensitive);
		Text.ReplaceInline(*(LocalizationPath + TEXT("\n") + LocalizationPath), *LocalizationPath, ESearchCase::CaseSensitive);
		if (Text != OriginalText && !SaveTextAtomically(Filename, Text))
		{
			OutResult.AddError(TEXT("NKMLOC_PATH_CONFIG_WRITE"), FString::Printf(TEXT("Unable to update path config '%s'."), *Filename));
			return false;
		}
		return true;
	}

	FString TargetConfigPath(const FString& TargetName, const FString& Suffix)
	{
		return FPaths::ProjectConfigDir() / FString::Printf(TEXT("Localization/%s_%s.ini"), *TargetName, *Suffix);
	}

	const TArray<FString>& GetTargetConfigSuffixes()
	{
		static const TArray<FString> Suffixes = {
			TEXT("Gather"), TEXT("Export"), TEXT("Import"), TEXT("Compile"),
			TEXT("ImportDialogue"), TEXT("ImportDialogueScript"),
			TEXT("ExportDialogueScript"), TEXT("GenerateReports")};
		return Suffixes;
	}

	bool PrepareTargetConfigs(
		const FString& PreviousTargetName,
		const FString& CurrentTargetName,
		FNKMLocalizationResult& OutResult)
	{
		for (const FString& Suffix : GetTargetConfigSuffixes())
		{
			const FString CurrentPath = TargetConfigPath(CurrentTargetName, Suffix);
			if (FPaths::FileExists(CurrentPath))
			{
				continue;
			}
			const FString PreviousPath = TargetConfigPath(PreviousTargetName, Suffix);
			if (!FPaths::FileExists(PreviousPath)
				|| IFileManager::Get().Copy(*CurrentPath, *PreviousPath, true, true) != COPY_OK)
			{
				OutResult.AddError(TEXT("NKMLOC_TARGET_CONFIG_CREATE"), FString::Printf(TEXT("Unable to create target config '%s' from '%s'."), *CurrentPath, *PreviousPath));
				return false;
			}
		}
		return true;
	}

	bool ReplaceTargetArtifacts(
		const FString& Filename,
		const FString& PreviousTargetName,
		const FString& CurrentTargetName,
		FNKMLocalizationResult& OutResult)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Filename))
		{
			OutResult.AddError(TEXT("NKMLOC_TARGET_CONFIG_READ"), FString::Printf(TEXT("Unable to read target config '%s'."), *Filename));
			return false;
		}
		const FString OriginalText = Text;
		const TArray<FString> Prefixes = {
			TEXT("ManifestName="), TEXT("ArchiveName="), TEXT("PortableObjectName="),
			TEXT("ResourceName="), TEXT("WordCountReportName="), TEXT("ConflictReportName=")};
		for (const FString& Prefix : Prefixes)
		{
			Text.ReplaceInline(*(Prefix + PreviousTargetName), *(Prefix + CurrentTargetName), ESearchCase::CaseSensitive);
		}
		if (Text != OriginalText && !SaveTextAtomically(Filename, Text))
		{
			OutResult.AddError(TEXT("NKMLOC_TARGET_CONFIG_WRITE"), FString::Printf(TEXT("Unable to update target identity in '%s'."), *Filename));
			return false;
		}
		return true;
	}

	bool RepairGatherConfig(
		const FString& Filename,
		const FString& StringTableContentRoot,
		const FString& TargetName,
		FNKMLocalizationResult& OutResult)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Filename))
		{
			OutResult.AddError(TEXT("NKMLOC_GATHER_CONFIG_READ"), FString::Printf(TEXT("Unable to read gather config '%s'."), *Filename));
			return false;
		}

		const int32 FirstStepIndex = Text.Find(TEXT("[GatherTextStep0]"), ESearchCase::CaseSensitive);
		if (FirstStepIndex == INDEX_NONE)
		{
			OutResult.AddError(TEXT("NKMLOC_GATHER_CONFIG_SCHEMA"), FString::Printf(TEXT("Gather config has no GatherTextStep0: '%s'."), *Filename));
			return false;
		}

		FString Repaired = Text.Left(FirstStepIndex);
		Repaired += FString::Printf(
			TEXT("[GatherTextStep0]\n")
			TEXT("CommandletClass=GatherTextFromAssets\n")
			TEXT("IncludePathFilters=%s/*\n")
			TEXT("ExcludePathFilters=Content/Localization/%s/*\n")
			TEXT("PackageFileNameFilters=*.uasset\n")
			TEXT("ShouldExcludeDerivedClasses=false\n")
			TEXT("ShouldGatherFromEditorOnlyData=false\n")
			TEXT("SkipGatherCache=false\n")
			TEXT("ReportStaleGatherCache=true\n")
			TEXT("FixStaleGatherCache=true\n")
			TEXT("FixMissingGatherCache=true\n\n")
			TEXT("[GatherTextStep1]\n")
			TEXT("CommandletClass=GenerateGatherManifest\n\n")
			TEXT("[GatherTextStep2]\n")
			TEXT("CommandletClass=GenerateGatherArchive\n\n")
			TEXT("[GatherTextStep3]\n")
			TEXT("CommandletClass=GenerateTextLocalizationReport\n")
			TEXT("bWordCountReport=true\n")
			TEXT("WordCountReportName=%s.csv\n")
			TEXT("bConflictReport=true\n")
			TEXT("ConflictReportName=%s_Conflicts.txt\n"),
			*StringTableContentRoot,
			*TargetName,
			*TargetName,
			*TargetName);
		if (Repaired != Text && !SaveTextAtomically(Filename, Repaired))
		{
			OutResult.AddError(TEXT("NKMLOC_GATHER_CONFIG_WRITE"), FString::Printf(TEXT("Unable to repair gather config '%s'."), *Filename));
			return false;
		}
		return true;
	}

	bool ValidateCultures(const UNKMLocalizationSettings& Settings, FNKMLocalizationResult& OutResult)
	{
		if (Settings.NativeCulture.IsEmpty() || Settings.SupportedCultures.IsEmpty()
			|| Settings.SupportedCultures[0] != Settings.NativeCulture)
		{
			OutResult.AddError(TEXT("NKMLOC_CULTURE_ORDER"), TEXT("Native Culture must be non-empty and first in Supported Cultures."));
			return false;
		}

		TSet<FString> UniqueCultures;
		for (const FString& Culture : Settings.SupportedCultures)
		{
			if (Culture.IsEmpty() || UniqueCultures.Contains(Culture)
				|| !FInternationalization::Get().GetCulture(Culture).IsValid())
			{
				OutResult.AddError(TEXT("NKMLOC_CULTURE_INVALID"), FString::Printf(TEXT("Unsupported, empty, or duplicate culture '%s'."), *Culture));
				return false;
			}
			UniqueCultures.Add(Culture);
		}
		return true;
	}

	bool ValidateBindings(const UNKMLocalizationSettings& Settings, FNKMLocalizationResult& OutResult)
	{
		TSet<FName> ProfileNames;
		for (const FNKMTextBindingProfile& Profile : Settings.TextBindingProfiles)
		{
			if (Profile.ProfileName.IsNone() || Profile.TableId.IsNone()
				|| !Settings.IsAllowedTableId(Profile.TableId.ToString())
				|| !Profile.KeyPattern.Contains(TEXT("{Id}"), ESearchCase::CaseSensitive)
				|| !Profile.KeyPattern.Contains(TEXT("{Field}"), ESearchCase::CaseSensitive)
				|| ProfileNames.Contains(Profile.ProfileName))
			{
				OutResult.AddError(TEXT("NKMLOC_BINDING_PROFILE"), FString::Printf(TEXT("Binding profile '%s' must be unique and define Table Id plus a Key Pattern containing {Id} and {Field}."), *Profile.ProfileName.ToString()));
				return false;
			}
			ProfileNames.Add(Profile.ProfileName);

			TSet<FName> FieldNames;
			for (const FNKMTextBindingField& Field : Profile.Fields)
			{
				if (Field.FieldName.IsNone() || FieldNames.Contains(Field.FieldName))
				{
					OutResult.AddError(TEXT("NKMLOC_BINDING_FIELD"), FString::Printf(TEXT("Binding profile '%s' contains an empty or duplicate field."), *Profile.ProfileName.ToString()));
					return false;
				}
				FieldNames.Add(Field.FieldName);
			}
			if (Profile.Fields.IsEmpty())
			{
				OutResult.AddError(TEXT("NKMLOC_BINDING_FIELDS"), FString::Printf(TEXT("Binding profile '%s' must define at least one field."), *Profile.ProfileName.ToString()));
				return false;
			}
			const bool bHasCsvProvider = !Profile.RecordSourceFile.FilePath.IsEmpty() && !Profile.RecordIdColumn.IsNone();
			const bool bHasReflectedProvider = !Profile.RecordSourceAsset.IsNull()
				&& !Profile.RecordMapProperty.IsNone()
				&& !Profile.RecordIdProperty.IsNone();
			if (!Profile.AuthoringSourceFile.IsEmpty() && !bHasCsvProvider && !bHasReflectedProvider)
			{
				OutResult.AddError(TEXT("NKMLOC_BINDING_PROVIDER"), FString::Printf(TEXT("Binding profile '%s' has an authoring source but no CSV or reflected record provider."), *Profile.ProfileName.ToString()));
				return false;
			}
		}
		return true;
	}

	bool ValidateIdentity(const UNKMLocalizationSettings& Settings, FNKMLocalizationResult& OutResult)
	{
		auto IsSafeIdentifier = [](const FString& Value)
		{
			if (Value.IsEmpty()) return false;
			for (const TCHAR Character : Value)
			{
				if (!FChar::IsAlnum(Character) && Character != TEXT('_')) return false;
			}
			return true;
		};
		if (!IsSafeIdentifier(Settings.LocalizationTargetName))
		{
			OutResult.AddError(TEXT("NKMLOC_TARGET_NAME"), TEXT("Localization Target Name must contain only letters, digits, and underscores."));
			return false;
		}
		if (!Settings.TableIdPrefix.IsEmpty() && !IsSafeIdentifier(Settings.TableIdPrefix))
		{
			OutResult.AddError(TEXT("NKMLOC_TABLE_PREFIX"), TEXT("Table Id Prefix must be empty or contain only letters, digits, and underscores."));
			return false;
		}
		if (Settings.StringTableAssetPrefix.IsEmpty())
		{
			OutResult.AddError(TEXT("NKMLOC_ASSET_PREFIX"), TEXT("String Table Asset Prefix cannot be empty."));
			return false;
		}
		return true;
	}

	bool ReplaceCultureBlock(
		const FString& Filename,
		const UNKMLocalizationSettings& Settings,
		FNKMLocalizationResult& OutResult)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Filename))
		{
			OutResult.AddError(TEXT("NKMLOC_CULTURE_CONFIG_READ"), FString::Printf(TEXT("Unable to read localization config '%s'."), *Filename));
			return false;
		}

		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, false);
		const int32 CommonIndex = Lines.IndexOfByKey(TEXT("[CommonSettings]"));
		if (CommonIndex == INDEX_NONE)
		{
			OutResult.AddError(TEXT("NKMLOC_CULTURE_CONFIG_SCHEMA"), FString::Printf(TEXT("Localization config has no CommonSettings section: '%s'."), *Filename));
			return false;
		}

		for (int32 Index = Lines.Num() - 1; Index > CommonIndex; --Index)
		{
			if (Lines[Index].StartsWith(TEXT("[")))
			{
				continue;
			}
			if (Lines[Index].StartsWith(TEXT("NativeCulture=")) || Lines[Index].StartsWith(TEXT("CulturesToGenerate=")))
			{
				Lines.RemoveAt(Index);
			}
		}

		int32 InsertIndex = CommonIndex + 1;
		Lines.Insert(TEXT("NativeCulture=") + Settings.NativeCulture, InsertIndex++);
		for (const FString& Culture : Settings.SupportedCultures)
		{
			Lines.Insert(TEXT("CulturesToGenerate=") + Culture, InsertIndex++);
		}

		const FString Updated = FString::Join(Lines, TEXT("\n")) + TEXT("\n");
		if (Updated != Text.Replace(TEXT("\r\n"), TEXT("\n")) && !SaveTextAtomically(Filename, Updated))
		{
			OutResult.AddError(TEXT("NKMLOC_CULTURE_CONFIG_WRITE"), FString::Printf(TEXT("Unable to update localization cultures in '%s'."), *Filename));
			return false;
		}
		return true;
	}

	bool ReplacePackagingCultures(
		const FString& Filename,
		const UNKMLocalizationSettings& Settings,
		FNKMLocalizationResult& OutResult)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Filename))
		{
			OutResult.AddError(TEXT("NKMLOC_PACKAGING_CONFIG_READ"), FString::Printf(TEXT("Unable to read packaging config '%s'."), *Filename));
			return false;
		}

		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, false);
		const FString Section = TEXT("[/Script/UnrealEd.ProjectPackagingSettings]");
		const int32 SectionIndex = Lines.IndexOfByKey(Section);
		if (SectionIndex == INDEX_NONE)
		{
			OutResult.AddError(TEXT("NKMLOC_PACKAGING_CONFIG_SCHEMA"), TEXT("DefaultGame.ini has no ProjectPackagingSettings section."));
			return false;
		}

		int32 SectionEnd = Lines.Num();
		for (int32 Index = SectionIndex + 1; Index < Lines.Num(); ++Index)
		{
			if (Lines[Index].StartsWith(TEXT("[")))
			{
				SectionEnd = Index;
				break;
			}
		}
		for (int32 Index = SectionEnd - 1; Index > SectionIndex; --Index)
		{
			if (Lines[Index].StartsWith(TEXT("CulturesToStage="))
				|| Lines[Index].StartsWith(TEXT("+CulturesToStage="))
				|| Lines[Index].StartsWith(TEXT("InternationalizationPreset=")))
			{
				Lines.RemoveAt(Index);
			}
		}

		int32 InsertIndex = SectionIndex + 1;
		Lines.Insert(TEXT("InternationalizationPreset=All"), InsertIndex++);
		for (const FString& Culture : Settings.SupportedCultures)
		{
			Lines.Insert(TEXT("+CulturesToStage=") + Culture, InsertIndex++);
		}

		const FString Updated = FString::Join(Lines, TEXT("\n")) + TEXT("\n");
		if (Updated != Text.Replace(TEXT("\r\n"), TEXT("\n")) && !SaveTextAtomically(Filename, Updated))
		{
			OutResult.AddError(TEXT("NKMLOC_PACKAGING_CONFIG_WRITE"), TEXT("Unable to update staged cultures in DefaultGame.ini."));
			return false;
		}
		return true;
	}

	bool ReplaceDashboardCultures(
		const FString& Filename,
		const UNKMLocalizationSettings& Settings,
		FNKMLocalizationResult& OutResult)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Filename))
		{
			OutResult.AddError(TEXT("NKMLOC_DASHBOARD_CONFIG_READ"), TEXT("Unable to read DefaultEditor.ini."));
			return false;
		}

		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, false);
		const FString CurrentNeedle = FString::Printf(TEXT("GameTargetsSettings=(Name=\"%s\""), *Settings.LocalizationTargetName);
		const FString PreviousNeedle = FString::Printf(TEXT("GameTargetsSettings=(Name=\"%s\""), *Settings.GetAppliedLocalizationTargetName());
		FString* TargetLine = Lines.FindByPredicate(
			[&CurrentNeedle, &PreviousNeedle](const FString& Line)
			{
				return Line.Contains(CurrentNeedle, ESearchCase::CaseSensitive)
					|| Line.Contains(PreviousNeedle, ESearchCase::CaseSensitive);
			});
		if (!TargetLine)
		{
			OutResult.AddError(TEXT("NKMLOC_DASHBOARD_TARGET"), FString::Printf(TEXT("DefaultEditor.ini has no '%s' localization target."), *Settings.LocalizationTargetName));
			return false;
		}
		TargetLine->ReplaceInline(*PreviousNeedle, *CurrentNeedle, ESearchCase::CaseSensitive);

		const int32 NativeIndex = TargetLine->Find(TEXT("NativeCultureIndex="), ESearchCase::CaseSensitive);
		const int32 CulturesIndex = TargetLine->Find(TEXT("SupportedCulturesStatistics="), ESearchCase::CaseSensitive);
		if (NativeIndex == INDEX_NONE || CulturesIndex == INDEX_NONE || CulturesIndex <= NativeIndex)
		{
			OutResult.AddError(TEXT("NKMLOC_DASHBOARD_TARGET_SCHEMA"), TEXT("Configured dashboard target has no culture settings."));
			return false;
		}

		const int32 NativeValueStart = NativeIndex + FCString::Strlen(TEXT("NativeCultureIndex="));
		const int32 NativeValueEnd = TargetLine->Find(TEXT(","), ESearchCase::CaseSensitive, ESearchDir::FromStart, NativeValueStart);
		if (NativeValueEnd == INDEX_NONE)
		{
			OutResult.AddError(TEXT("NKMLOC_DASHBOARD_TARGET_SCHEMA"), TEXT("Configured dashboard target NativeCultureIndex is malformed."));
			return false;
		}
		*TargetLine = TargetLine->Left(NativeValueStart) + TEXT("0") + TargetLine->Mid(NativeValueEnd);

		const int32 UpdatedCulturesIndex = TargetLine->Find(TEXT("SupportedCulturesStatistics="), ESearchCase::CaseSensitive);
		FString Statistics = TEXT("SupportedCulturesStatistics=(");
		for (int32 Index = 0; Index < Settings.SupportedCultures.Num(); ++Index)
		{
			if (Index > 0) Statistics += TEXT(",");
			Statistics += FString::Printf(TEXT("(CultureName=\"%s\")"), *Settings.SupportedCultures[Index]);
		}
		Statistics += TEXT(")");
		*TargetLine = TargetLine->Left(UpdatedCulturesIndex) + Statistics + TEXT(")");

		const FString Updated = FString::Join(Lines, TEXT("\n")) + TEXT("\n");
		if (Updated != Text.Replace(TEXT("\r\n"), TEXT("\n")) && !SaveTextAtomically(Filename, Updated))
		{
			OutResult.AddError(TEXT("NKMLOC_DASHBOARD_CONFIG_WRITE"), TEXT("Unable to update configured target cultures in DefaultEditor.ini."));
			return false;
		}
		return true;
	}
}

FString FNKMLocalizationPathConfigurator::ToProjectContentPath(const FString& LongPackageRoot)
{
	return LongPackageRoot.StartsWith(TEXT("/Game"))
		? TEXT("Content") + LongPackageRoot.RightChop(5)
		: FString();
}

bool FNKMLocalizationPathConfigurator::Apply(FNKMLocalizationResult& OutResult)
{
	UNKMLocalizationSettings* Settings = GetMutableDefault<UNKMLocalizationSettings>();
	if (!ValidateIdentity(*Settings, OutResult) || !ValidateCultures(*Settings, OutResult) || !ValidateBindings(*Settings, OutResult))
	{
		return false;
	}
	const FString CurrentRoot = Settings->GetNormalizedStringTableAssetRoot();
	if (!FPackageName::IsValidLongPackageName(CurrentRoot) || !CurrentRoot.StartsWith(TEXT("/Game/")))
	{
		OutResult.AddError(TEXT("NKMLOC_PATH_ROOT"), TEXT("String Table Asset Root must be a valid /Game package path."));
		return false;
	}
	const FString CurrentTargetRoot = Settings->GetNormalizedLocalizationTargetRoot();
	const FString StandardTargetRoot = TEXT("Content/Localization") / Settings->LocalizationTargetName;
	if (CurrentTargetRoot != StandardTargetRoot)
	{
		OutResult.AddError(TEXT("NKMLOC_PATH_TARGET_ROOT"), TEXT("Localization Target Root must use Unreal's standard Content/Localization/{TargetName} layout."));
		return false;
	}

	FString PreviousRoot = Settings->GetAppliedStringTableAssetRoot();
	if (PreviousRoot.IsEmpty()) PreviousRoot = TEXT("/Game/NKM/Localization/StringTables");
	while (PreviousRoot.EndsWith(TEXT("/"))) PreviousRoot.LeftChopInline(1);
	FString PreviousTargetRoot = Settings->GetAppliedLocalizationTargetRoot();
	if (PreviousTargetRoot.IsEmpty()) PreviousTargetRoot = TEXT("Content/Localization/NKMText");
	FPaths::NormalizeDirectoryName(PreviousTargetRoot);
	FString PreviousTargetName = Settings->GetAppliedLocalizationTargetName();
	if (PreviousTargetName.IsEmpty()) PreviousTargetName = Settings->LocalizationTargetName;
	if (!PrepareTargetConfigs(PreviousTargetName, Settings->LocalizationTargetName, OutResult))
	{
		return false;
	}

	TArray<FString> ConfigFiles = {
		FPaths::ProjectConfigDir() / TEXT("DefaultGame.ini"),
		FPaths::ProjectConfigDir() / TEXT("DefaultEditor.ini"),
		FPaths::ProjectConfigDir() / TEXT("Localization/Game_Gather.ini")};
	for (const FString& Suffix : GetTargetConfigSuffixes())
	{
		ConfigFiles.Add(TargetConfigPath(Settings->LocalizationTargetName, Suffix));
	}
	for (const FString& ConfigFile : ConfigFiles)
	{
		if (!ReplaceManagedRoot(ConfigFile, PreviousRoot, CurrentRoot, PreviousTargetRoot, CurrentTargetRoot, OutResult))
		{
			return false;
		}
	}
	for (const FString& Suffix : GetTargetConfigSuffixes())
	{
		if (!ReplaceTargetArtifacts(TargetConfigPath(Settings->LocalizationTargetName, Suffix), PreviousTargetName, Settings->LocalizationTargetName, OutResult))
		{
			return false;
		}
	}
	if (!RepairGatherConfig(
		TargetConfigPath(Settings->LocalizationTargetName, TEXT("Gather")),
		ToProjectContentPath(CurrentRoot),
		Settings->LocalizationTargetName,
		OutResult))
	{
		return false;
	}
	const TArray<FString> CultureConfigs = {
		TargetConfigPath(Settings->LocalizationTargetName, TEXT("Gather")),
		TargetConfigPath(Settings->LocalizationTargetName, TEXT("Export")),
		TargetConfigPath(Settings->LocalizationTargetName, TEXT("Import")),
		TargetConfigPath(Settings->LocalizationTargetName, TEXT("Compile"))};
	for (const FString& CultureConfig : CultureConfigs)
	{
		if (!ReplaceCultureBlock(CultureConfig, *Settings, OutResult))
		{
			return false;
		}
	}
	if (!ReplacePackagingCultures(FPaths::ProjectConfigDir() / TEXT("DefaultGame.ini"), *Settings, OutResult))
	{
		return false;
	}
	if (!ReplaceDashboardCultures(FPaths::ProjectConfigDir() / TEXT("DefaultEditor.ini"), *Settings, OutResult))
	{
		return false;
	}

	Settings->SetAppliedStringTableAssetRoot(CurrentRoot);
	Settings->SetAppliedLocalizationTargetRoot(CurrentTargetRoot);
	Settings->SetAppliedLocalizationTargetName(Settings->LocalizationTargetName);
	if (!Settings->TryUpdateDefaultConfigFile())
	{
		OutResult.AddError(TEXT("NKMLOC_PATH_SETTINGS_WRITE"), TEXT("Unable to persist the applied NKM Localization path settings."));
		return false;
	}
	const FString DefaultGamePath = FPaths::ProjectConfigDir() / TEXT("DefaultGame.ini");
	FString DefaultGameText;
	if (FFileHelper::LoadFileToString(DefaultGameText, *DefaultGamePath))
	{
		const FString OriginalDefaultGameText = DefaultGameText;
		DefaultGameText.TrimEndInline();
		DefaultGameText += LINE_TERMINATOR;
		if (DefaultGameText != OriginalDefaultGameText && !SaveTextAtomically(DefaultGamePath, DefaultGameText))
		{
			OutResult.AddError(TEXT("NKMLOC_PATH_SETTINGS_NORMALIZE"), TEXT("Unable to normalize DefaultGame.ini after saving path settings."));
			return false;
		}
	}
	return true;
}
