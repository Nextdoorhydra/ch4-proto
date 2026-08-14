#pragma once

#include "DataForgeRenameAdvisor.h"

struct FDataForgeRenameRecoveryRecord
{
	FString Filename;
	FString Operation = TEXT("RenameAuditBatch");
	FString CreatedUtc;
	TArray<FDataForgeRenameCandidate> Candidates;
};

struct FDataForgeRenameRecoveryManifest
{
	FString Filename;
	FString State;
	FString CreatedUtc;
	FString UpdatedUtc;
	FString Message;
	TArray<FDataForgeRenameCandidate> Candidates;
};

class FDataForgeRenameRecovery
{
public:
	static bool Begin(
		const TArray<FDataForgeRenameCandidate>& Candidates,
		FDataForgeRenameRecoveryRecord& OutRecord,
		FString& OutError);

	static bool Update(
		const FDataForgeRenameRecoveryRecord& Record,
		const FString& State,
		bool bRollbackAttempted,
		bool bRollbackSucceeded,
		const FString& Message,
		FString& OutError);

	static TArray<FDataForgeRenameRecoveryManifest> LoadRecent(int32 Limit, TArray<FDataForgeDiagnostic>& OutDiagnostics);
	static FDataForgeResult ValidateRestore(const FDataForgeRenameRecoveryManifest& Manifest);
	static FDataForgeResult Restore(const FDataForgeRenameRecoveryManifest& Manifest);
};
