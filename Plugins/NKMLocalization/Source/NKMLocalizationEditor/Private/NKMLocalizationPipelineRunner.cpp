#include "NKMLocalizationPipelineRunner.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Hash/Blake3.h"
#include "Misc/DateTime.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "NKMLocalizationSettings.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	constexpr int32 LedgerSchemaVersion = 1;
	constexpr int32 ReportSchemaVersion = 1;

	FString GetTargetName()
	{
		return GetDefault<UNKMLocalizationSettings>()->LocalizationTargetName;
	}

	FString GetArtifactName(const FString& Extension)
	{
		return GetTargetName() + Extension;
	}

	FString GetConfigRelativePath(const FString& Stage)
	{
		return FString::Printf(TEXT("Config/Localization/%s_%s.ini"), *GetTargetName(), *Stage);
	}

	FString ProjectPath(const FString& RelativePath)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), RelativePath);
	}

	FString GetLedgerPath()
	{
		return ProjectPath(FString::Printf(TEXT("Localization/State/%sPoLedger.json"), *GetTargetName()));
	}

	FString GetTargetPath(const FString& RelativePath)
	{
		return ProjectPath(GetDefault<UNKMLocalizationSettings>()->GetNormalizedLocalizationTargetRoot() / RelativePath);
	}

	bool HashFile(const FString& Path, FString& OutHash)
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Path))
		{
			return false;
		}

		OutHash = LexToString(FBlake3::HashBuffer(Bytes.GetData(), Bytes.Num()));
		return true;
	}

	bool BuildPOSnapshot(TMap<FString, FString>& OutSnapshot, FNKMLocalizationResult& Result)
	{
		for (const FString& Culture : FNKMLocalizationPipelineRunner::GetCultures())
		{
			const FString RelativePath = Culture / GetArtifactName(TEXT(".po"));
			const FString AbsolutePath = GetTargetPath(RelativePath);
			if (!FPaths::FileExists(AbsolutePath))
			{
				continue;
			}

			FString Hash;
			if (!HashFile(AbsolutePath, Hash))
			{
				Result.AddError(TEXT("NKMLOC-PO-READ"), FString::Printf(TEXT("Failed to read PO file '%s'."), *AbsolutePath));
				return false;
			}

			OutSnapshot.Add(RelativePath, MoveTemp(Hash));
		}

		return true;
	}

	bool LoadLedger(TMap<FString, FString>& OutSnapshot, FNKMLocalizationResult& Result)
	{
		FString Json;
		const FString LedgerPath = GetLedgerPath();
		if (!FFileHelper::LoadFileToString(Json, *LedgerPath))
		{
			Result.AddError(TEXT("NKMLOC-PO-LEDGER-READ"), FString::Printf(TEXT("Failed to read PO ledger '%s'."), *LedgerPath));
			return false;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			Result.AddError(TEXT("NKMLOC-PO-LEDGER-JSON"), FString::Printf(TEXT("PO ledger '%s' is not valid JSON."), *LedgerPath));
			return false;
		}

		double SchemaVersion = 0.0;
		FString Target;
		if (!Root->TryGetNumberField(TEXT("schemaVersion"), SchemaVersion)
			|| static_cast<int32>(SchemaVersion) != LedgerSchemaVersion
			|| !Root->TryGetStringField(TEXT("target"), Target)
			|| Target != GetTargetName()
			|| !Root->HasTypedField<EJson::Object>(TEXT("files")))
		{
			Result.AddError(TEXT("NKMLOC-PO-LEDGER-SCHEMA"), FString::Printf(TEXT("PO ledger '%s' has an unsupported schema or target."), *LedgerPath));
			return false;
		}

		const TSharedPtr<FJsonObject> Files = Root->GetObjectField(TEXT("files"));
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Files->Values)
		{
			FString Hash;
			if (!Pair.Value.IsValid() || !Pair.Value->TryGetString(Hash))
			{
				Result.AddError(TEXT("NKMLOC-PO-LEDGER-SCHEMA"), FString::Printf(TEXT("PO ledger entry '%s' is not a hash string."), *Pair.Key));
				return false;
			}
			OutSnapshot.Add(Pair.Key, MoveTemp(Hash));
		}

		return true;
	}

	bool SaveJsonAtomically(const FString& Path, const FString& Json)
	{
		const FString Directory = FPaths::GetPath(Path);
		if (!IFileManager::Get().MakeDirectory(*Directory, true))
		{
			return false;
		}

		const FString TemporaryPath = Path + TEXT(".tmp");
		if (!FFileHelper::SaveStringToFile(Json, *TemporaryPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			return false;
		}

		if (!IFileManager::Get().Move(*Path, *TemporaryPath, true, true, false, true))
		{
			IFileManager::Get().Delete(*TemporaryPath, false, true);
			return false;
		}

		return true;
	}

	bool IsNonEmptyFile(const FString& Path)
	{
		return FPaths::FileExists(Path) && IFileManager::Get().FileSize(*Path) > 0;
	}

	bool ReadTargetConfig(
		const FString& ConfigRelativePath,
		FString& OutNativeCulture,
		TArray<FString>& OutCultures,
		FNKMLocalizationResult& Result)
	{
		const FString ConfigPath = FConfigCacheIni::NormalizeConfigIniPath(ProjectPath(ConfigRelativePath));
		if (!FPaths::FileExists(ConfigPath)
			|| !GConfig->GetString(TEXT("CommonSettings"), TEXT("NativeCulture"), OutNativeCulture, ConfigPath)
			|| GConfig->GetArray(TEXT("CommonSettings"), TEXT("CulturesToGenerate"), OutCultures, ConfigPath) == 0)
		{
			Result.AddError(TEXT("NKMLOC-CONFIG-SCHEMA"), FString::Printf(TEXT("Localization config has no NativeCulture or CulturesToGenerate: '%s'."), *ConfigPath));
			return false;
		}
		return true;
	}

	bool ValidateTargetConfigs(FNKMLocalizationResult& Result)
	{
		const TArray<FString> Configs = {
			GetConfigRelativePath(TEXT("Gather")),
			GetConfigRelativePath(TEXT("Export")),
			GetConfigRelativePath(TEXT("Import")),
			GetConfigRelativePath(TEXT("Compile"))};

		FString ExpectedNativeCulture;
		TArray<FString> ExpectedCultures;
		if (!ReadTargetConfig(Configs[0], ExpectedNativeCulture, ExpectedCultures, Result))
		{
			return false;
		}

		const UNKMLocalizationSettings* Settings = GetDefault<UNKMLocalizationSettings>();
		TSet<FString> UniqueCultures;
		for (const FString& Culture : ExpectedCultures)
		{
			UniqueCultures.Add(Culture);
		}
		if (ExpectedNativeCulture != Settings->NativeCulture
			|| ExpectedCultures != Settings->SupportedCultures
			|| UniqueCultures.Num() != ExpectedCultures.Num())
		{
			Result.AddError(TEXT("NKMLOC-CULTURE-CONFIG"), FString::Printf(TEXT("%s cultures differ from Project Settings > NKM Localization."), *GetTargetName()));
			return false;
		}

		for (int32 ConfigIndex = 1; ConfigIndex < Configs.Num(); ++ConfigIndex)
		{
			FString NativeCulture;
			TArray<FString> Cultures;
			if (!ReadTargetConfig(Configs[ConfigIndex], NativeCulture, Cultures, Result))
			{
				return false;
			}
			if (NativeCulture != ExpectedNativeCulture || Cultures != ExpectedCultures)
			{
				Result.AddError(
					TEXT("NKMLOC-CULTURE-CONFIG-DRIFT"),
					FString::Printf(TEXT("NativeCulture/CulturesToGenerate differs from %s_Gather.ini: '%s'."), *GetTargetName(), *ProjectPath(Configs[ConfigIndex])));
				return false;
			}
		}

		const FString ExpectedTargetRoot = GetDefault<UNKMLocalizationSettings>()->GetNormalizedLocalizationTargetRoot();
		for (const FString& Config : Configs)
		{
			const FString ConfigPath = FConfigCacheIni::NormalizeConfigIniPath(ProjectPath(Config));
			FString SourcePath;
			FString DestinationPath;
			if (!GConfig->GetString(TEXT("CommonSettings"), TEXT("SourcePath"), SourcePath, ConfigPath)
				|| !GConfig->GetString(TEXT("CommonSettings"), TEXT("DestinationPath"), DestinationPath, ConfigPath)
				|| SourcePath != ExpectedTargetRoot
				|| DestinationPath != ExpectedTargetRoot)
			{
				Result.AddError(
					TEXT("NKMLOC-TARGET-PATH-DRIFT"),
					FString::Printf(TEXT("Localization target paths differ from NKM Localization settings: '%s'."), *ConfigPath));
				return false;
			}
		}

		return true;
	}

	bool VerifyArtifact(const FString& RelativePath, FNKMLocalizationResult& Result)
	{
		const FString AbsolutePath = GetTargetPath(RelativePath);
		if (!IsNonEmptyFile(AbsolutePath))
		{
			Result.AddError(TEXT("NKMLOC-ARTIFACT-MISSING"), FString::Printf(TEXT("Required localization artifact is missing or empty: '%s'."), *AbsolutePath));
			return false;
		}
		return true;
	}

	bool JsonObjectContainsFieldRecursive(const TSharedPtr<FJsonObject>& Object, const FString& FieldName)
	{
		if (!Object.IsValid())
		{
			return false;
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
		{
			if (Pair.Key == FieldName)
			{
				return true;
			}
			if (!Pair.Value.IsValid())
			{
				continue;
			}

			if (Pair.Value->Type == EJson::Object && JsonObjectContainsFieldRecursive(Pair.Value->AsObject(), FieldName))
			{
				return true;
			}
			if (Pair.Value->Type == EJson::Array)
			{
				for (const TSharedPtr<FJsonValue>& ArrayValue : Pair.Value->AsArray())
				{
					if (ArrayValue.IsValid()
						&& ArrayValue->Type == EJson::Object
						&& JsonObjectContainsFieldRecursive(ArrayValue->AsObject(), FieldName))
					{
						return true;
					}
				}
			}
		}

		return false;
	}

	bool VerifyManifestContainsEntries(FNKMLocalizationResult& Result)
	{
		const FString ManifestPath = GetTargetPath(GetArtifactName(TEXT(".manifest")));
		FString Json;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Json, *ManifestPath))
		{
			Result.AddError(
				TEXT("NKMLOC-MANIFEST-EMPTY"),
				FString::Printf(TEXT("%s manifest has no gathered text entries: '%s'."), *GetTargetName(), *ManifestPath));
			return false;
		}

		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root)
			|| !JsonObjectContainsFieldRecursive(Root, TEXT("Keys")))
		{
			Result.AddError(
				TEXT("NKMLOC-MANIFEST-EMPTY"),
				FString::Printf(TEXT("%s manifest has no gathered text entries: '%s'."), *GetTargetName(), *ManifestPath));
			return false;
		}
		return true;
	}

	bool VerifyGatherArtifacts(FNKMLocalizationResult& Result)
	{
		bool bValid = VerifyArtifact(GetArtifactName(TEXT(".manifest")), Result);
		bValid = VerifyManifestContainsEntries(Result) && bValid;
		for (const FString& Culture : FNKMLocalizationPipelineRunner::GetCultures())
		{
			bValid = VerifyArtifact(Culture / GetArtifactName(TEXT(".archive")), Result) && bValid;
			bValid = VerifyArtifact(Culture / GetArtifactName(TEXT(".po")), Result) && bValid;
		}
		return bValid;
	}

	bool VerifyCompiledArtifacts(FNKMLocalizationResult& Result)
	{
		bool bValid = VerifyGatherArtifacts(Result);
		bValid = VerifyArtifact(GetArtifactName(TEXT(".locmeta")), Result) && bValid;
		for (const FString& Culture : FNKMLocalizationPipelineRunner::GetCultures())
		{
			bValid = VerifyArtifact(Culture / GetArtifactName(TEXT(".locres")), Result) && bValid;
		}
		return bValid;
	}

	bool VerifyNoGatherConflicts(FNKMLocalizationResult& Result)
	{
		const FString ConflictPath = GetTargetPath(GetArtifactName(TEXT("_Conflicts.txt")));
		const int64 Size = IFileManager::Get().FileSize(*ConflictPath);
		if (Size > 0)
		{
			Result.AddError(TEXT("NKMLOC-GATHER-CONFLICT"), FString::Printf(TEXT("Gather conflict report is not empty: '%s'."), *ConflictPath));
			return false;
		}
		if (Size == -1)
		{
			Result.AddError(TEXT("NKMLOC-GATHER-CONFLICT-REPORT"), FString::Printf(TEXT("Gather did not produce conflict report '%s'."), *ConflictPath));
			return false;
		}
		return true;
	}

	bool RunGatherTextStage(
		const FString& Name,
		const FString& ConfigRelativePath,
		FNKMLocalizationResult& Result,
		FNKMLocalizationPipelineReport& Report)
	{
		const FString StringTableAssetRoot = GetDefault<UNKMLocalizationSettings>()->GetNormalizedStringTableAssetRoot();
		FAssetRegistryModule::GetRegistry().ScanPathsSynchronous({StringTableAssetRoot}, true);

		const FString ConfigPath = ProjectPath(ConfigRelativePath);
		FNKMLocalizationStageReport& Stage = Report.Stages.AddDefaulted_GetRef();
		Stage.Name = Name;
		Stage.ConfigPath = ConfigRelativePath;

		if (!FPaths::FileExists(ConfigPath))
		{
			Stage.ExitCode = 1;
			Result.AddError(TEXT("NKMLOC-CONFIG-MISSING"), FString::Printf(TEXT("Localization config does not exist: '%s'."), *ConfigPath));
			return false;
		}

		const double StartedAt = FPlatformTime::Seconds();
		const FString ProjectFile = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
		const FString Params = FString::Printf(
			TEXT("\"%s\" -run=GatherText -Config=\"%s\" -DisableSCCSubmit -unattended -nop4"),
			*ProjectFile,
			*ConfigPath);
		FString StdOut;
		FString StdErr;
		Stage.ExitCode = 1;
		const bool bStarted = FPlatformProcess::ExecProcess(
			FPlatformProcess::ExecutablePath(),
			*Params,
			&Stage.ExitCode,
			&StdOut,
			&StdErr);
		Stage.DurationSeconds = FPlatformTime::Seconds() - StartedAt;
		if (!bStarted)
		{
			Result.AddError(TEXT("NKMLOC-STAGE-LAUNCH-FAILED"), FString::Printf(TEXT("Failed to launch localization stage '%s': %s"), *Name, *StdErr));
			return false;
		}
		if (Stage.ExitCode != 0)
		{
			Result.AddError(TEXT("NKMLOC-STAGE-FAILED"), FString::Printf(TEXT("Localization stage '%s' failed with exit code %d."), *Name, Stage.ExitCode));
			return false;
		}

		return true;
	}
}

void FNKMLocalizationPOLedger::FindChangedFiles(
	const TMap<FString, FString>& Expected,
	const TMap<FString, FString>& Current,
	TArray<FString>& OutChangedFiles)
{
	TSet<FString> AllFiles;
	for (const TPair<FString, FString>& Pair : Expected)
	{
		AllFiles.Add(Pair.Key);
	}
	for (const TPair<FString, FString>& Pair : Current)
	{
		AllFiles.Add(Pair.Key);
	}

	for (const FString& File : AllFiles)
	{
		const FString* ExpectedHash = Expected.Find(File);
		const FString* CurrentHash = Current.Find(File);
		if (!ExpectedHash || !CurrentHash || *ExpectedHash != *CurrentHash)
		{
			OutChangedFiles.Add(File);
		}
	}
	OutChangedFiles.Sort();
}

bool FNKMLocalizationPOLedger::CheckCurrentState(FNKMLocalizationResult& Result)
{
	TMap<FString, FString> Current;
	if (!BuildPOSnapshot(Current, Result))
	{
		return false;
	}

	if (Current.IsEmpty())
	{
		return true;
	}

	if (!FPaths::FileExists(GetLedgerPath()))
	{
		Result.AddError(
			TEXT("NKMLOC-PO-LEDGER-MISSING"),
			TEXT("PO files exist without an import/export ledger. Run ImportCompile first, or use -ForcePOOverwrite only when discarding those PO edits is intentional."));
		return false;
	}

	TMap<FString, FString> Expected;
	if (!LoadLedger(Expected, Result))
	{
		return false;
	}

	TArray<FString> ChangedFiles;
	FindChangedFiles(Expected, Current, ChangedFiles);
	if (!ChangedFiles.IsEmpty())
	{
		Result.AddError(
			TEXT("NKMLOC-PO-UNIMPORTED"),
			FString::Printf(
				TEXT("PO files changed since the last successful ImportCompile or GatherExport: %s. Run ImportCompile before exporting, or use -ForcePOOverwrite only to discard edits."),
				*FString::Join(ChangedFiles, TEXT(", "))));
		return false;
	}

	return true;
}

bool FNKMLocalizationPOLedger::Update(FNKMLocalizationResult& Result)
{
	TMap<FString, FString> Snapshot;
	if (!BuildPOSnapshot(Snapshot, Result))
	{
		return false;
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schemaVersion"), LedgerSchemaVersion);
	Root->SetStringField(TEXT("target"), GetTargetName());
	Root->SetStringField(TEXT("hashAlgorithm"), TEXT("BLAKE3-256"));
	Root->SetStringField(TEXT("updatedAtUtc"), FDateTime::UtcNow().ToIso8601());

	TSharedRef<FJsonObject> Files = MakeShared<FJsonObject>();
	TArray<FString> Paths;
	Snapshot.GetKeys(Paths);
	Paths.Sort();
	for (const FString& Path : Paths)
	{
		Files->SetStringField(Path, Snapshot.FindChecked(Path));
	}
	Root->SetObjectField(TEXT("files"), Files);

	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	if (!FJsonSerializer::Serialize(Root, Writer) || !SaveJsonAtomically(GetLedgerPath(), Json))
	{
		Result.AddError(TEXT("NKMLOC-PO-LEDGER-WRITE"), FString::Printf(TEXT("Failed to write PO ledger '%s'."), *GetLedgerPath()));
		return false;
	}

	return true;
}

bool FNKMLocalizationPipelineRunner::Run(
	const FString& Mode,
	const bool bForcePOOverwrite,
	FNKMLocalizationResult& Result,
	FNKMLocalizationPipelineReport& Report)
{
	Report.Mode = Mode;
	Report.StartedAtUtc = FDateTime::UtcNow().ToIso8601();
	if (!ValidateTargetConfigs(Result))
	{
		Report.bSucceeded = false;
		Report.FinishedAtUtc = FDateTime::UtcNow().ToIso8601();
		return false;
	}

	bool bSucceeded = false;
	if (Mode.Equals(TEXT("GatherExport"), ESearchCase::IgnoreCase))
	{
		const bool bPOStateSafe = bForcePOOverwrite || FNKMLocalizationPOLedger::CheckCurrentState(Result);
		if (bForcePOOverwrite)
		{
			Result.AddWarning(TEXT("NKMLOC-PO-FORCE-OVERWRITE"), TEXT("PO overwrite protection was explicitly bypassed."));
		}

		bSucceeded = bPOStateSafe
			&& RunGatherTextStage(TEXT("Gather"), GetConfigRelativePath(TEXT("Gather")), Result, Report)
			&& VerifyNoGatherConflicts(Result)
			&& RunGatherTextStage(TEXT("Export"), GetConfigRelativePath(TEXT("Export")), Result, Report)
			&& VerifyGatherArtifacts(Result)
			&& FNKMLocalizationPOLedger::Update(Result);
	}
	else if (Mode.Equals(TEXT("ImportCompile"), ESearchCase::IgnoreCase))
	{
		bSucceeded = VerifyGatherArtifacts(Result)
			&& RunGatherTextStage(TEXT("Import"), GetConfigRelativePath(TEXT("Import")), Result, Report)
			&& RunGatherTextStage(TEXT("Compile"), GetConfigRelativePath(TEXT("Compile")), Result, Report)
			&& VerifyCompiledArtifacts(Result)
			&& FNKMLocalizationPOLedger::Update(Result);
	}
	else if (Mode.Equals(TEXT("Verify"), ESearchCase::IgnoreCase))
	{
		bSucceeded = VerifyNoGatherConflicts(Result)
			&& VerifyCompiledArtifacts(Result)
			&& FNKMLocalizationPOLedger::CheckCurrentState(Result);
	}
	else
	{
		Result.AddError(TEXT("NKMLOC-MODE"), FString::Printf(TEXT("Unsupported pipeline mode '%s'."), *Mode));
	}

	Report.bSucceeded = bSucceeded && !Result.HasErrors();
	Report.FinishedAtUtc = FDateTime::UtcNow().ToIso8601();
	return Report.bSucceeded;
}

bool FNKMLocalizationPipelineRunner::WriteReport(
	const FString& ReportPath,
	const FNKMLocalizationPipelineReport& Report,
	const FNKMLocalizationResult& Result)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schemaVersion"), ReportSchemaVersion);
	Root->SetStringField(TEXT("target"), GetTargetName());
	Root->SetStringField(TEXT("mode"), Report.Mode);
	Root->SetBoolField(TEXT("succeeded"), Report.bSucceeded);
	Root->SetStringField(TEXT("startedAtUtc"), Report.StartedAtUtc);
	Root->SetStringField(TEXT("finishedAtUtc"), Report.FinishedAtUtc);

	TArray<TSharedPtr<FJsonValue>> Stages;
	for (const FNKMLocalizationStageReport& Stage : Report.Stages)
	{
		TSharedRef<FJsonObject> StageJson = MakeShared<FJsonObject>();
		StageJson->SetStringField(TEXT("name"), Stage.Name);
		StageJson->SetStringField(TEXT("configPath"), Stage.ConfigPath);
		StageJson->SetNumberField(TEXT("exitCode"), Stage.ExitCode);
		StageJson->SetNumberField(TEXT("durationSeconds"), Stage.DurationSeconds);
		Stages.Add(MakeShared<FJsonValueObject>(StageJson));
	}
	Root->SetArrayField(TEXT("stages"), Stages);

	TArray<TSharedPtr<FJsonValue>> Diagnostics;
	for (const FNKMLocalizationDiagnostic& Diagnostic : Result.Diagnostics)
	{
		TSharedRef<FJsonObject> DiagnosticJson = MakeShared<FJsonObject>();
		DiagnosticJson->SetStringField(
			TEXT("severity"),
			Diagnostic.Severity == ENKMLocalizationDiagnosticSeverity::Error ? TEXT("error") : TEXT("warning"));
		DiagnosticJson->SetStringField(TEXT("code"), Diagnostic.Code);
		DiagnosticJson->SetStringField(TEXT("message"), Diagnostic.Message);
		DiagnosticJson->SetStringField(TEXT("tableId"), Diagnostic.TableId);
		DiagnosticJson->SetStringField(TEXT("key"), Diagnostic.Key);
		Diagnostics.Add(MakeShared<FJsonValueObject>(DiagnosticJson));
	}
	Root->SetArrayField(TEXT("diagnostics"), Diagnostics);

	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	return FJsonSerializer::Serialize(Root, Writer) && SaveJsonAtomically(ReportPath, Json);
}

TArray<FString> FNKMLocalizationPipelineRunner::GetCultures()
{
	return GetDefault<UNKMLocalizationSettings>()->SupportedCultures;
}
