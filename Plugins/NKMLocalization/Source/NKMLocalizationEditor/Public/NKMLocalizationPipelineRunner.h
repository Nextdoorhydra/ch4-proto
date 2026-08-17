#pragma once

#include "CoreMinimal.h"
#include "NKMLocalizationTypes.h"

struct FNKMLocalizationStageReport
{
	FString Name;
	FString ConfigPath;
	int32 ExitCode = INDEX_NONE;
	double DurationSeconds = 0.0;
};

struct FNKMLocalizationPipelineReport
{
	FString Mode;
	FString StartedAtUtc;
	FString FinishedAtUtc;
	bool bSucceeded = false;
	TArray<FNKMLocalizationStageReport> Stages;
};

class NKMLOCALIZATIONEDITOR_API FNKMLocalizationPOLedger
{
public:
	static bool CheckCurrentState(FNKMLocalizationResult& Result);
	static bool Update(FNKMLocalizationResult& Result);

	/** Pure comparison helper used by automation tests and future UI previews. */
	static void FindChangedFiles(
		const TMap<FString, FString>& Expected,
		const TMap<FString, FString>& Current,
		TArray<FString>& OutChangedFiles);
};

class NKMLOCALIZATIONEDITOR_API FNKMLocalizationPipelineRunner
{
public:
	static bool Run(
		const FString& Mode,
		bool bForcePOOverwrite,
		FNKMLocalizationResult& Result,
		FNKMLocalizationPipelineReport& Report);

	static bool WriteReport(
		const FString& ReportPath,
		const FNKMLocalizationPipelineReport& Report,
		const FNKMLocalizationResult& Result);

	/** Reads the authoritative culture list configured for the current localization target. */
	static TArray<FString> GetCultures();
};
