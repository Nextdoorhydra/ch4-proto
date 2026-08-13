#include "DataForgeRenameRecovery.h"

#include "Dom/JsonObject.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	void AddRecoveryDiagnostic(TArray<FDataForgeDiagnostic>& Diagnostics, EDataForgeSeverity Severity, const TCHAR* Code, const FString& Message)
	{
		FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = Severity;
		Diagnostic.Code = Code;
		Diagnostic.Message = Message;
	}

	UObject* ResolveObject(const FSoftObjectPath& Path)
	{
		if (UObject* Existing = Path.ResolveObject()) return Existing;
		return FPackageName::DoesPackageExist(Path.GetLongPackageName()) ? Path.TryLoad() : nullptr;
	}

	bool WriteRecord(
		const FDataForgeRenameRecoveryRecord& Record,
		const FString& State,
		bool bRollbackAttempted,
		bool bRollbackSucceeded,
		const FString& Message,
		FString& OutError)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("formatVersion"), 1);
		Root->SetStringField(TEXT("operation"), Record.Operation);
		Root->SetStringField(TEXT("state"), State);
		Root->SetStringField(TEXT("createdUtc"), Record.CreatedUtc);
		Root->SetStringField(TEXT("updatedUtc"), FDateTime::UtcNow().ToIso8601());
		Root->SetBoolField(TEXT("rollbackAttempted"), bRollbackAttempted);
		Root->SetBoolField(TEXT("rollbackSucceeded"), bRollbackSucceeded);
		Root->SetStringField(TEXT("message"), Message);

		TArray<TSharedPtr<FJsonValue>> Entries;
		for (const FDataForgeRenameCandidate& Candidate : Record.Candidates)
		{
			TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("sourceObjectPath"), Candidate.AssetPath.ToString());
			Entry->SetStringField(TEXT("destinationObjectPath"), Candidate.GetSuggestedObjectPath());
			Entry->SetStringField(TEXT("ruleSet"), Candidate.RuleSetPath.ToString());
			Entry->SetStringField(TEXT("recordId"), Candidate.RecordId.ToString());
			Entry->SetStringField(TEXT("slotId"), Candidate.SlotId.ToString());
			Entry->SetNumberField(TEXT("matchScore"), Candidate.MatchScore);
			TArray<TSharedPtr<FJsonValue>> Evidence;
			for (const FString& Item : Candidate.MatchEvidence) Evidence.Add(MakeShared<FJsonValueString>(Item));
			Entry->SetArrayField(TEXT("matchEvidence"), Evidence);
			Entries.Add(MakeShared<FJsonValueObject>(Entry));
		}
		Root->SetArrayField(TEXT("entries"), Entries);

		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		if (!FJsonSerializer::Serialize(Root, Writer))
		{
			OutError = TEXT("Could not serialize the rename recovery manifest.");
			return false;
		}

		const FString TemporaryFilename = Record.Filename + TEXT(".tmp");
		if (!FFileHelper::SaveStringToFile(Json, *TemporaryFilename))
		{
			OutError = FString::Printf(TEXT("Could not write temporary recovery manifest: %s"), *TemporaryFilename);
			return false;
		}
		if (!IFileManager::Get().Move(*Record.Filename, *TemporaryFilename, true, true))
		{
			IFileManager::Get().Delete(*TemporaryFilename, false, true);
			OutError = FString::Printf(TEXT("Could not finalize recovery manifest: %s"), *Record.Filename);
			return false;
		}
		return true;
	}

	bool BeginRecord(
		const FString& Prefix,
		const FString& Operation,
		const TArray<FDataForgeRenameCandidate>& Candidates,
		FDataForgeRenameRecoveryRecord& OutRecord,
		FString& OutError)
	{
		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DataForge"), TEXT("Recovery"));
		if (!IFileManager::Get().MakeDirectory(*Directory, true))
		{
			OutError = FString::Printf(TEXT("Could not create recovery directory: %s"), *Directory);
			return false;
		}
		OutRecord.Operation = Operation;
		OutRecord.CreatedUtc = FDateTime::UtcNow().ToIso8601();
		OutRecord.Candidates = Candidates;
		OutRecord.Filename = FPaths::Combine(Directory, FString::Printf(
			TEXT("%s_%s_%s.json"), *Prefix,
			*FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		return WriteRecord(OutRecord, TEXT("Planned"), false, false, TEXT("Validated asset path operation has not started."), OutError);
	}

	TArray<FDataForgeRenameCandidate> BuildReverseCandidates(const FDataForgeRenameRecoveryManifest& Manifest)
	{
		TArray<FDataForgeRenameCandidate> Reverse;
		for (const FDataForgeRenameCandidate& Original : Manifest.Candidates)
		{
			if (!ResolveObject(FSoftObjectPath(Original.GetSuggestedObjectPath()))) continue;
			FDataForgeRenameCandidate& Candidate = Reverse.AddDefaulted_GetRef();
			Candidate.AssetPath = FSoftObjectPath(Original.GetSuggestedObjectPath());
			Candidate.RuleSetPath = Original.RuleSetPath;
			Candidate.RecordId = Original.RecordId;
			Candidate.SlotId = Original.SlotId;
			Candidate.SuggestedPackageName = Original.AssetPath.GetLongPackageName();
			Candidate.SuggestedAssetName = FPackageName::ObjectPathToObjectName(Original.AssetPath.ToString());
			Candidate.MatchEvidence.Add(TEXT("Recovery manifest reverse operation"));
		}
		return Reverse;
	}
}

bool FDataForgeRenameRecovery::Begin(
	const TArray<FDataForgeRenameCandidate>& Candidates,
	FDataForgeRenameRecoveryRecord& OutRecord,
	FString& OutError)
{
	return BeginRecord(TEXT("Rename"), TEXT("RenameAuditBatch"), Candidates, OutRecord, OutError);
}

bool FDataForgeRenameRecovery::Update(
	const FDataForgeRenameRecoveryRecord& Record,
	const FString& State,
	bool bRollbackAttempted,
	bool bRollbackSucceeded,
	const FString& Message,
	FString& OutError)
{
	return WriteRecord(Record, State, bRollbackAttempted, bRollbackSucceeded, Message, OutError);
}

TArray<FDataForgeRenameRecoveryManifest> FDataForgeRenameRecovery::LoadRecent(
	int32 Limit,
	TArray<FDataForgeDiagnostic>& OutDiagnostics)
{
	TArray<FDataForgeRenameRecoveryManifest> Result;
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DataForge"), TEXT("Recovery"));
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(Directory, TEXT("Rename_*.json")), true, false);
	Files.Sort(TGreater<FString>());
	for (const FString& File : Files)
	{
		if (Result.Num() >= FMath::Max(1, Limit)) break;
		const FString Filename = FPaths::Combine(Directory, File);
		FString Json;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Json, *Filename)
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root)
			|| !Root.IsValid())
		{
			AddRecoveryDiagnostic(OutDiagnostics, EDataForgeSeverity::Warning, TEXT("DF1957"),
				FString::Printf(TEXT("Could not read recovery manifest: %s"), *Filename));
			continue;
		}
		FString Operation;
		if (!Root->TryGetStringField(TEXT("operation"), Operation) || Operation != TEXT("RenameAuditBatch")) continue;

		FDataForgeRenameRecoveryManifest& Manifest = Result.AddDefaulted_GetRef();
		Manifest.Filename = Filename;
		Root->TryGetStringField(TEXT("state"), Manifest.State);
		Root->TryGetStringField(TEXT("createdUtc"), Manifest.CreatedUtc);
		Root->TryGetStringField(TEXT("updatedUtc"), Manifest.UpdatedUtc);
		Root->TryGetStringField(TEXT("message"), Manifest.Message);
		const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
		if (!Root->TryGetArrayField(TEXT("entries"), Entries) || !Entries) continue;
		for (const TSharedPtr<FJsonValue>& Value : *Entries)
		{
			const TSharedPtr<FJsonObject> Entry = Value ? Value->AsObject() : nullptr;
			if (!Entry) continue;
			FString SourcePath;
			FString DestinationPath;
			if (!Entry->TryGetStringField(TEXT("sourceObjectPath"), SourcePath)
				|| !Entry->TryGetStringField(TEXT("destinationObjectPath"), DestinationPath)) continue;
			FDataForgeRenameCandidate& Candidate = Manifest.Candidates.AddDefaulted_GetRef();
			Candidate.AssetPath = FSoftObjectPath(SourcePath);
			Candidate.SuggestedPackageName = FPackageName::ObjectPathToPackageName(DestinationPath);
			Candidate.SuggestedAssetName = FPackageName::ObjectPathToObjectName(DestinationPath);
			FString Text;
			if (Entry->TryGetStringField(TEXT("ruleSet"), Text)) Candidate.RuleSetPath = FSoftObjectPath(Text);
			if (Entry->TryGetStringField(TEXT("recordId"), Text)) Candidate.RecordId = FName(*Text);
			if (Entry->TryGetStringField(TEXT("slotId"), Text)) Candidate.SlotId = FName(*Text);
			double MatchScore = 0.0;
			if (Entry->TryGetNumberField(TEXT("matchScore"), MatchScore)) Candidate.MatchScore = static_cast<int32>(MatchScore);
			const TArray<TSharedPtr<FJsonValue>>* Evidence = nullptr;
			if (Entry->TryGetArrayField(TEXT("matchEvidence"), Evidence) && Evidence)
			{
				for (const TSharedPtr<FJsonValue>& EvidenceValue : *Evidence)
				{
					FString EvidenceText;
					if (EvidenceValue && EvidenceValue->TryGetString(EvidenceText)) Candidate.MatchEvidence.Add(EvidenceText);
				}
			}
		}
	}
	return Result;
}

FDataForgeResult FDataForgeRenameRecovery::ValidateRestore(const FDataForgeRenameRecoveryManifest& Manifest)
{
	FDataForgeResult Result;
	if (Manifest.State != TEXT("Succeeded") && Manifest.State != TEXT("FailedRollbackIncomplete"))
	{
		AddRecoveryDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1958"),
			FString::Printf(TEXT("Manifest state '%s' cannot be restored."), *Manifest.State));
		return Result;
	}
	int32 RestoreCount = 0;
	for (const FDataForgeRenameCandidate& Candidate : Manifest.Candidates)
	{
		UObject* Source = ResolveObject(Candidate.AssetPath);
		UObject* Destination = ResolveObject(FSoftObjectPath(Candidate.GetSuggestedObjectPath()));
		if (Source && Destination)
		{
			AddRecoveryDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1959"),
				FString::Printf(TEXT("Restore destination is occupied: %s"), *Candidate.AssetPath.ToString()));
		}
		else if (!Source && !Destination)
		{
			AddRecoveryDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1960"),
				FString::Printf(TEXT("Neither recorded path contains the asset: %s"), *Candidate.GetSuggestedObjectPath()));
		}
		else if (Destination)
		{
			++RestoreCount;
		}
	}
	if (Result.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Severity == EDataForgeSeverity::Error;
	})) return Result;
	if (RestoreCount == 0)
	{
		AddRecoveryDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1961"),
			TEXT("Every entry is already at its original path."));
		return Result;
	}
	Result.bSuccess = true;
	Result.Summary = FString::Printf(TEXT("%d asset(s) can be restored."), RestoreCount);
	return Result;
}

FDataForgeResult FDataForgeRenameRecovery::Restore(const FDataForgeRenameRecoveryManifest& Manifest)
{
	FDataForgeResult Result = ValidateRestore(Manifest);
	if (!Result.bSuccess) return Result;
	const TArray<FDataForgeRenameCandidate> Reverse = BuildReverseCandidates(Manifest);
	FDataForgeRenameRecoveryRecord RestoreRecord;
	FString Error;
	if (!BeginRecord(TEXT("Restore"), TEXT("RenameRecoveryRestore"), Reverse, RestoreRecord, Error))
	{
		Result.bSuccess = false;
		AddRecoveryDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1962"), Error);
		return Result;
	}

	TArray<FAssetRenameData> RenameData;
	TArray<UObject*> Assets;
	for (const FDataForgeRenameCandidate& Candidate : Reverse)
	{
		UObject* Asset = ResolveObject(Candidate.AssetPath);
		Assets.Add(Asset);
		RenameData.Emplace(
			Asset,
			FPackageName::GetLongPackagePath(Candidate.SuggestedPackageName),
			Candidate.SuggestedAssetName);
	}
	if (!FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().RenameAssets(RenameData))
	{
		Result.bSuccess = false;
		AddRecoveryDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF1963"),
			TEXT("Unreal AssetTools rejected the restore batch."));
		TArray<FAssetRenameData> RollForwardData;
		bool bRollbackSucceeded = true;
		for (int32 Index = 0; Index < Reverse.Num(); ++Index)
		{
			const FString CurrentPath = Assets[Index] ? Assets[Index]->GetPathName() : FString();
			if (CurrentPath == Reverse[Index].AssetPath.ToString()) continue;
			if (CurrentPath != Reverse[Index].GetSuggestedObjectPath())
			{
				bRollbackSucceeded = false;
				continue;
			}
			RollForwardData.Emplace(
				Assets[Index],
				FPackageName::GetLongPackagePath(Reverse[Index].AssetPath.GetLongPackageName()),
				FPackageName::ObjectPathToObjectName(Reverse[Index].AssetPath.ToString()));
		}
		const bool bRollbackAttempted = !RollForwardData.IsEmpty();
		if (bRollbackAttempted
			&& !FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().RenameAssets(RollForwardData))
		{
			bRollbackSucceeded = false;
		}
		for (int32 Index = 0; Index < Reverse.Num(); ++Index)
		{
			bRollbackSucceeded &= Assets[Index] && Assets[Index]->GetPathName() == Reverse[Index].AssetPath.ToString();
		}
		const FString FailureMessage = bRollbackSucceeded
			? TEXT("Restore failed; every observed move was returned to its pre-restore path.")
			: TEXT("Restore failed and rollback is incomplete. Inspect the recorded paths before continuing.");
		Update(RestoreRecord,
			bRollbackSucceeded ? TEXT("FailedRolledBack") : TEXT("FailedRollbackIncomplete"),
			bRollbackAttempted, bRollbackSucceeded, FailureMessage, Error);
		Result.Summary = FailureMessage + TEXT(" Recovery: ") + RestoreRecord.Filename;
		return Result;
	}

	if (!Update(RestoreRecord, TEXT("Succeeded"), false, false,
		FString::Printf(TEXT("Restored %d asset(s)."), Reverse.Num()), Error))
	{
		AddRecoveryDiagnostic(Result.Diagnostics, EDataForgeSeverity::Warning, TEXT("DF1964"), Error);
	}
	FDataForgeRenameRecoveryRecord OriginalRecord;
	OriginalRecord.Filename = Manifest.Filename;
	OriginalRecord.CreatedUtc = Manifest.CreatedUtc;
	OriginalRecord.Candidates = Manifest.Candidates;
	if (!Update(OriginalRecord, TEXT("Restored"), false, false,
		FString::Printf(TEXT("Restored by %s"), *RestoreRecord.Filename), Error))
	{
		AddRecoveryDiagnostic(Result.Diagnostics, EDataForgeSeverity::Warning, TEXT("DF1965"), Error);
	}
	Result.bSuccess = true;
	Result.Summary = FString::Printf(TEXT("Restored %d asset(s). Recovery: %s"), Reverse.Num(), *RestoreRecord.Filename);
	return Result;
}
