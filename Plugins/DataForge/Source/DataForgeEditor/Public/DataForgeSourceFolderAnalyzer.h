#pragma once

#include "CoreMinimal.h"
#include "DataForgeAuthoringTypes.h"
#include "DataForgeTypes.h"

class UDataForgeNamingPolicy;

struct DATAFORGEEDITOR_API FDataForgePrimaryKeyCandidate
{
	FName Column = NAME_None;
	int32 NonEmptyCount = 0;
	int32 UniqueCount = 0;
	float NonEmptyRatio = 0.0f;
	float UniqueRatio = 0.0f;
	int32 NamePriority = 0;
};

struct DATAFORGEEDITOR_API FDataForgePrimaryKeyAnalysis
{
	TArray<FDataForgePrimaryKeyCandidate> Candidates;
	FDataForgeAuthoringDecision Decision;
	TArray<FDataForgeDiagnostic> Diagnostics;
};

/** Raw registry observation consumed before a Folder Source Config exists. */
struct DATAFORGEEDITOR_API FDataForgeFolderAssetObservation
{
	FString ObjectPath;
	FString PackagePath;
	FString AssetClassPath;
	FName AssetKind = NAME_None;
};

struct DATAFORGEEDITOR_API FDataForgeFolderLayoutCandidate
{
	int32 SubjectFolderIndex = INDEX_NONE;
	int32 KindFolderIndex = INDEX_NONE;
	float SubjectCoverage = 0.0f;
	float SourceKeyCoverage = 0.0f;
	float KindCoverage = 0.0f;
	float CombinedScore = 0.0f;
	FString Pattern;
};

struct DATAFORGEEDITOR_API FDataForgeFolderLayoutAnalysis
{
	FString RootFolder;
	TArray<FDataForgeFolderLayoutCandidate> Candidates;
	FDataForgeAuthoringDecision Decision;
	TArray<FDataForgeDiagnostic> Diagnostics;
};

/** Mutation-free source and raw-folder inference used by the authoring planner. */
class DATAFORGEEDITOR_API FDataForgeSourceFolderAnalyzer
{
public:
	static FDataForgePrimaryKeyAnalysis AnalyzePrimaryKey(
		const FDataForgeDataSet& DataSet,
		FName PreferredPrimaryKey = NAME_None);

	static FDataForgeFolderLayoutAnalysis AnalyzeFolderLayout(
		const FString& RootFolder,
		const FDataForgeDataSet& PrimaryData,
		FName SourceKeyColumn,
		const TArray<FDataForgeFolderAssetObservation>& Observations);

	static bool ScanFolder(
		const FString& RootFolder,
		const UDataForgeNamingPolicy* NamingPolicy,
		TArray<FDataForgeFolderAssetObservation>& OutObservations,
		TArray<FDataForgeDiagnostic>& OutDiagnostics);
};
