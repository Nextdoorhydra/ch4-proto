#pragma once

#include "CoreMinimal.h"
#include "DataForgeAuthoringTypes.h"
#include "DataForgeBindingPreset.h"

class UScriptStruct;

struct DATAFORGEEDITOR_API FDataForgeReflectedSlotCandidate
{
	FName SlotId = NAME_None;
	FName AssetKind = NAME_None;
	FName Role = NAME_None;
	FString TargetProperty;
	TSoftClassPtr<UObject> ExpectedAssetClass;
	EDataForgeBindingCardinality Cardinality = EDataForgeBindingCardinality::OptionalOne;
	EDataForgeBindingReconcileMode Reconcile = EDataForgeBindingReconcileMode::Assign;
	bool bRequired = false;
	EDataForgeInferenceDisposition Disposition = EDataForgeInferenceDisposition::Unresolved;
	TArray<FDataForgeInferenceEvidence> Evidence;
};

/** Mutation-free reflection result for one requested generated output. */
struct DATAFORGEEDITOR_API FDataForgeOutputReflectionAnalysis
{
	FName OutputName = NAME_None;
	TWeakObjectPtr<UClass> TargetClass;
	TArray<FDataForgeReflectedSlotCandidate> Slots;
	FDataForgeAuthoringDecision RowReferenceDecision;
	TArray<FDataForgeDiagnostic> Diagnostics;
};

/** Column spelling is preserved so materialization can write the exact source contract. */
struct DATAFORGEEDITOR_API FDataForgeAssociationSemanticMapping
{
	FName MatchColumn = NAME_None;
	FName AssetPathColumn = NAME_None;
	FName AssetKindColumn = NAME_None;
	FName RoleColumn = NAME_None;
	EDataForgeInferenceDisposition Disposition = EDataForgeInferenceDisposition::Unresolved;
	TArray<FDataForgeInferenceEvidence> Evidence;
	TArray<FDataForgeDiagnostic> Diagnostics;
};

class DATAFORGEEDITOR_API FDataForgeOutputReflectionAnalyzer
{
public:
	static FDataForgeOutputReflectionAnalysis Analyze(
		UClass* TargetClass,
		FName OutputName,
		UScriptStruct* RowStruct,
		const TArray<FDataForgeAssignmentHint>& AssignmentHints = {});

	static FDataForgeAssociationSemanticMapping ResolveAssociationSemantics(
		FName AdapterId,
		const TArray<FName>& Columns);
};
