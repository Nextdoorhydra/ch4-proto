#pragma once

#include "CoreMinimal.h"
#include "DataForgeTypes.h"
#include "Engine/DataAsset.h"

class UScriptStruct;

enum class EDataForgeInferenceDisposition : uint8
{
	Unresolved,
	Exact,
	Recommended,
	Ambiguous,
	Unsupported,
	Conflict
};

enum class EDataForgeAuthoringArtifactAction : uint8
{
	Reuse,
	Create
};

struct DATAFORGEEDITOR_API FDataForgeRequestedOutput
{
	FName OutputName = NAME_None;
	TSoftClassPtr<UDataAsset> AssetClass;
	FString OutputFolder;
	FString RowReferenceProperty;
};

struct DATAFORGEEDITOR_API FDataForgeAssignmentHint
{
	FName AssetKind = NAME_None;
	FName Role = NAME_None;
	FName OutputName = NAME_None;
	FString TargetProperty;
};

/** Minimal user intent shared by the editor and future MCP authoring entry points. */
struct DATAFORGEEDITOR_API FDataForgeAuthoringIntent
{
	FDataForgeSourceConfig Source;
	FName PreferredPrimaryKey = NAME_None;
	TArray<FString> AssetSearchRoots;
	TWeakObjectPtr<UScriptStruct> RowStruct;
	FString DataTablePath;
	TArray<FDataForgeRequestedOutput> Outputs;
	TArray<FDataForgeAssignmentHint> AssignmentHints;

	void Normalize();
};

struct DATAFORGEEDITOR_API FDataForgeInferenceEvidence
{
	FName Kind = NAME_None;
	FString Value;
	FString Source;
};

struct DATAFORGEEDITOR_API FDataForgeAuthoringDecision
{
	FName DecisionId = NAME_None;
	EDataForgeInferenceDisposition Disposition = EDataForgeInferenceDisposition::Unresolved;
	bool bRequired = true;
	FString SelectedValue;
	TArray<FString> Alternatives;
	TArray<FDataForgeInferenceEvidence> Evidence;
};

struct DATAFORGEEDITOR_API FDataForgeAuthoringArtifact
{
	FName ArtifactKind = NAME_None;
	EDataForgeAuthoringArtifactAction Action = EDataForgeAuthoringArtifactAction::Create;
	FString ObjectPath;
};

/** Mutation-free analysis result. Materialization is intentionally a later authoring phase. */
struct DATAFORGEEDITOR_API FDataForgeAuthoringPlan
{
	FDataForgeAuthoringIntent Intent;
	TArray<FDataForgeAuthoringDecision> Decisions;
	TArray<FDataForgeAuthoringArtifact> Artifacts;
	TArray<FDataForgeDiagnostic> Diagnostics;

	void Normalize();
	bool HasBlockingIssues() const;
	FString MakeStableSignature() const;
};
