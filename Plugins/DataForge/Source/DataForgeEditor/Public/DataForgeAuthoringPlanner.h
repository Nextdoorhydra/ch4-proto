#pragma once

#include "CoreMinimal.h"
#include "DataForgeAuthoringTypes.h"
#include "DataForgeOutputReflectionAnalyzer.h"
#include "DataForgeSourceFolderAnalyzer.h"

/** Probe/scan results are injected so planning stays deterministic and performs no content writes. */
struct DATAFORGEEDITOR_API FDataForgeObservedAssetRoot
{
	FString RootFolder;
	TArray<FDataForgeFolderAssetObservation> Observations;
};

struct DATAFORGEEDITOR_API FDataForgeAssociationSchema
{
	FName SourceId = NAME_None;
	FName AdapterId = NAME_None;
	/** Complete source configuration when the association is not synthesized from an analyzed asset root. */
	FDataForgeSourceConfig Source;
	TArray<FName> Columns;
};

struct DATAFORGEEDITOR_API FDataForgeResolvedAssociationSchema
{
	FDataForgeAssociationSchema Schema;
	FDataForgeAssociationSemanticMapping Mapping;
};

struct DATAFORGEEDITOR_API FDataForgeAuthoringPlannerRequest
{
	FDataForgeAuthoringIntent Intent;
	FDataForgeDataSet PrimaryData;
	TArray<FDataForgeObservedAssetRoot> AssetRoots;
	TArray<FDataForgeAssociationSchema> AssociationSchemas;
};

struct DATAFORGEEDITOR_API FDataForgeAuthoringPlannerResult
{
	FDataForgeAuthoringPlan Plan;
	TArray<FName> SourceColumns;
	FDataForgePrimaryKeyAnalysis PrimaryKey;
	TArray<FDataForgeFolderLayoutAnalysis> FolderLayouts;
	TArray<FDataForgeOutputReflectionAnalysis> Outputs;
	TArray<FDataForgeResolvedAssociationSchema> Associations;
};

class DATAFORGEEDITOR_API FDataForgeAuthoringPlanner
{
public:
	static FDataForgeAuthoringPlannerResult BuildPlan(const FDataForgeAuthoringPlannerRequest& Request);
};
