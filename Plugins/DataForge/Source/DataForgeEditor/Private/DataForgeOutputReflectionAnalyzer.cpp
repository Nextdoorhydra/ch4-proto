#include "DataForgeOutputReflectionAnalyzer.h"

#include "Engine/DataAsset.h"
#include "UObject/UnrealType.h"

namespace DataForgeOutputReflectionAnalyzer
{
	void AddDiagnostic(TArray<FDataForgeDiagnostic>& Diagnostics, EDataForgeSeverity Severity,
		const TCHAR* Code, const FString& Message, FName Field = NAME_None)
	{
		FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = Severity;
		Diagnostic.Code = Code;
		Diagnostic.Message = Message;
		Diagnostic.Field = Field;
	}

	bool IsAuthoringProperty(const FProperty* Property)
	{
		return Property
			&& Property->HasAnyPropertyFlags(CPF_Edit)
			&& !Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated);
	}

	FSoftObjectProperty* ResolveSoftReference(FProperty* Property, EDataForgeBindingCardinality& OutCardinality)
	{
		if (FSoftObjectProperty* SoftObject = CastField<FSoftObjectProperty>(Property))
		{
			OutCardinality = EDataForgeBindingCardinality::OptionalOne;
			return SoftObject;
		}
		if (FArrayProperty* Array = CastField<FArrayProperty>(Property))
		{
			if (FSoftObjectProperty* Inner = CastField<FSoftObjectProperty>(Array->Inner))
			{
				OutCardinality = EDataForgeBindingCardinality::Many;
				return Inner;
			}
		}
		return nullptr;
	}

	FName InferAssetKind(const FProperty* Property, const UClass* ReferenceClass)
	{
		const FString Metadata = Property->GetMetaData(TEXT("DataForgeKind"));
		if (!Metadata.IsEmpty()) return FName(*Metadata);
		const FString ClassName = ReferenceClass ? ReferenceClass->GetName() : FString();
		if (ClassName.Contains(TEXT("Texture"), ESearchCase::IgnoreCase)) return TEXT("Texture");
		if (ClassName.Contains(TEXT("Material"), ESearchCase::IgnoreCase)) return TEXT("Material");
		if (ClassName.Contains(TEXT("SkeletalMesh"), ESearchCase::IgnoreCase)) return TEXT("SkeletalMesh");
		if (ClassName.Contains(TEXT("StaticMesh"), ESearchCase::IgnoreCase)) return TEXT("StaticMesh");
		if (ClassName.Contains(TEXT("Mesh"), ESearchCase::IgnoreCase)) return TEXT("Mesh");
		if (ClassName.Contains(TEXT("Niagara"), ESearchCase::IgnoreCase)) return TEXT("NiagaraSystem");
		if (ClassName.Contains(TEXT("Blueprint"), ESearchCase::IgnoreCase)) return TEXT("Blueprint");
		return NAME_None;
	}

	FName InferRole(const FProperty* Property, FName AssetKind)
	{
		const FString Metadata = Property->GetMetaData(TEXT("DataForgeRole"));
		if (!Metadata.IsEmpty()) return FName(*Metadata);
		FString Stem = Property->GetName();
		if (Stem.EndsWith(TEXT("s"), ESearchCase::IgnoreCase)) Stem.LeftChopInline(1);
		if (!AssetKind.IsNone() && Stem.Equals(AssetKind.ToString(), ESearchCase::IgnoreCase)) return AssetKind;
		return FName(*Stem);
	}

	bool IsCompatibleRowReference(FProperty* Property, UClass* TargetClass)
	{
		EDataForgeBindingCardinality Cardinality = EDataForgeBindingCardinality::One;
		FSoftObjectProperty* Reference = ResolveSoftReference(Property, Cardinality);
		return Reference && Cardinality != EDataForgeBindingCardinality::Many && TargetClass
			&& TargetClass->IsChildOf(Reference->PropertyClass);
	}

	TArray<FName> FindAliases(const TArray<FName>& Columns, std::initializer_list<const TCHAR*> Aliases)
	{
		TArray<FName> Matches;
		for (const TCHAR* Alias : Aliases)
		{
			for (const FName Column : Columns)
			{
				if (Column.ToString().Equals(Alias, ESearchCase::IgnoreCase))
				{
					Matches.AddUnique(Column);
				}
			}
		}
		return Matches;
	}

	void AddSemanticEvidence(TArray<FDataForgeInferenceEvidence>& Evidence, const TCHAR* Semantic, FName Column)
	{
		if (Column.IsNone()) return;
		FDataForgeInferenceEvidence& Item = Evidence.AddDefaulted_GetRef();
		Item.Kind = Semantic;
		Item.Value = Column.ToString();
		Item.Source = TEXT("AssociationSource.Columns");
	}
}

FDataForgeOutputReflectionAnalysis FDataForgeOutputReflectionAnalyzer::Analyze(
	UClass* TargetClass,
	FName OutputName,
	UScriptStruct* RowStruct,
	const TArray<FDataForgeAssignmentHint>& AssignmentHints)
{
	FDataForgeOutputReflectionAnalysis Analysis;
	Analysis.OutputName = OutputName;
	Analysis.TargetClass = TargetClass;
	Analysis.RowReferenceDecision.DecisionId = FName(*FString::Printf(TEXT("Output.%s.RowReference"), *OutputName.ToString()));
	Analysis.RowReferenceDecision.bRequired = RowStruct != nullptr;

	if (!TargetClass || !TargetClass->IsChildOf(UDataAsset::StaticClass()))
	{
		Analysis.RowReferenceDecision.Disposition = EDataForgeInferenceDisposition::Unsupported;
		DataForgeOutputReflectionAnalyzer::AddDiagnostic(Analysis.Diagnostics, EDataForgeSeverity::Error, TEXT("DF2030"),
			TEXT("Generated Output reflection requires a DataAsset-derived target class."), OutputName);
		return Analysis;
	}

	for (TFieldIterator<FProperty> It(TargetClass, EFieldIterationFlags::IncludeSuper); It; ++It)
	{
		FProperty* Property = *It;
		if (!DataForgeOutputReflectionAnalyzer::IsAuthoringProperty(Property)) continue;
		EDataForgeBindingCardinality Cardinality = EDataForgeBindingCardinality::One;
		FSoftObjectProperty* Reference = DataForgeOutputReflectionAnalyzer::ResolveSoftReference(Property, Cardinality);
		if (!Reference) continue;

		FDataForgeReflectedSlotCandidate Candidate;
		Candidate.SlotId = Property->GetFName();
		Candidate.TargetProperty = Property->GetName();
		Candidate.ExpectedAssetClass = Reference->PropertyClass.Get();
		Candidate.Cardinality = Cardinality;
		Candidate.Reconcile = Cardinality == EDataForgeBindingCardinality::Many
			? EDataForgeBindingReconcileMode::ReplaceManaged
			: EDataForgeBindingReconcileMode::Assign;
		Candidate.AssetKind = DataForgeOutputReflectionAnalyzer::InferAssetKind(Property, Reference->PropertyClass);
		Candidate.Role = DataForgeOutputReflectionAnalyzer::InferRole(Property, Candidate.AssetKind);
		Candidate.Disposition = Candidate.AssetKind.IsNone()
			? EDataForgeInferenceDisposition::Unsupported
			: EDataForgeInferenceDisposition::Recommended;

		if (!Property->GetMetaData(TEXT("DataForgeKind")).IsEmpty() || !Property->GetMetaData(TEXT("DataForgeRole")).IsEmpty())
		{
			Candidate.Disposition = EDataForgeInferenceDisposition::Exact;
		}
		for (const FDataForgeAssignmentHint& Hint : AssignmentHints)
		{
			if (!Hint.OutputName.IsNone() && Hint.OutputName != OutputName) continue;
			if (!Hint.TargetProperty.Equals(Candidate.TargetProperty, ESearchCase::IgnoreCase)) continue;
			if (!Hint.AssetKind.IsNone()) Candidate.AssetKind = Hint.AssetKind;
			if (!Hint.Role.IsNone()) Candidate.Role = Hint.Role;
			Candidate.Disposition = EDataForgeInferenceDisposition::Exact;
			FDataForgeInferenceEvidence& Evidence = Candidate.Evidence.AddDefaulted_GetRef();
			Evidence.Kind = TEXT("AssignmentHint");
			Evidence.Value = Candidate.TargetProperty;
			Evidence.Source = TEXT("AuthoringIntent");
		}
		if (Candidate.AssetKind.IsNone())
		{
			DataForgeOutputReflectionAnalyzer::AddDiagnostic(Analysis.Diagnostics, EDataForgeSeverity::Warning, TEXT("DF2031"),
				FString::Printf(TEXT("Property '%s' is a supported soft reference, but its Asset Kind cannot be inferred."), *Candidate.TargetProperty), Property->GetFName());
		}
		Analysis.Slots.Add(MoveTemp(Candidate));
	}
	Analysis.Slots.Sort([](const FDataForgeReflectedSlotCandidate& A, const FDataForgeReflectedSlotCandidate& B)
	{
		return A.TargetProperty < B.TargetProperty;
	});

	if (!RowStruct)
	{
		Analysis.RowReferenceDecision.bRequired = false;
		Analysis.RowReferenceDecision.Disposition = EDataForgeInferenceDisposition::Unresolved;
		return Analysis;
	}

	TArray<FProperty*> Compatible;
	for (TFieldIterator<FProperty> It(RowStruct, EFieldIterationFlags::IncludeSuper); It; ++It)
	{
		if (DataForgeOutputReflectionAnalyzer::IsAuthoringProperty(*It)
			&& DataForgeOutputReflectionAnalyzer::IsCompatibleRowReference(*It, TargetClass))
		{
			Compatible.Add(*It);
			Analysis.RowReferenceDecision.Alternatives.Add((*It)->GetName());
		}
	}
	Compatible.Sort([](const FProperty& A, const FProperty& B) { return A.GetName() < B.GetName(); });
	Analysis.RowReferenceDecision.Alternatives.Sort();
	FProperty** ExactMatch = Compatible.FindByPredicate([OutputName](const FProperty* Property)
	{
		return Property->GetName().Equals(OutputName.ToString(), ESearchCase::IgnoreCase);
	});
	FProperty* Exact = ExactMatch ? *ExactMatch : nullptr;
	if (Exact)
	{
		Analysis.RowReferenceDecision.SelectedValue = Exact->GetName();
		Analysis.RowReferenceDecision.Disposition = EDataForgeInferenceDisposition::Exact;
	}
	else if (Compatible.Num() == 1)
	{
		Analysis.RowReferenceDecision.SelectedValue = Compatible[0]->GetName();
		Analysis.RowReferenceDecision.Disposition = EDataForgeInferenceDisposition::Recommended;
	}
	else if (Compatible.Num() > 1)
	{
		Analysis.RowReferenceDecision.Disposition = EDataForgeInferenceDisposition::Ambiguous;
		DataForgeOutputReflectionAnalyzer::AddDiagnostic(Analysis.Diagnostics, EDataForgeSeverity::Warning, TEXT("DF2032"),
			FString::Printf(TEXT("Output '%s' has multiple compatible DataTable Row reference properties."), *OutputName.ToString()), OutputName);
	}
	else
	{
		Analysis.RowReferenceDecision.Disposition = EDataForgeInferenceDisposition::Unsupported;
		DataForgeOutputReflectionAnalyzer::AddDiagnostic(Analysis.Diagnostics, EDataForgeSeverity::Error, TEXT("DF2033"),
			FString::Printf(TEXT("Output '%s' has no compatible DataTable Row reference property."), *OutputName.ToString()), OutputName);
	}
	return Analysis;
}

FDataForgeAssociationSemanticMapping FDataForgeOutputReflectionAnalyzer::ResolveAssociationSemantics(
	FName AdapterId,
	const TArray<FName>& Columns)
{
	using namespace DataForgeOutputReflectionAnalyzer;
	FDataForgeAssociationSemanticMapping Result;
	const TArray<FName> Match = FindAliases(Columns, { TEXT("Subject"), TEXT("Id"), TEXT("ID"), TEXT("Key"), TEXT("MatchKey") });
	const TArray<FName> Path = FindAliases(Columns, { TEXT("ObjectPath"), TEXT("AssetPath"), TEXT("Path") });
	const TArray<FName> Kind = FindAliases(Columns, { TEXT("AssetKind"), TEXT("Kind"), TEXT("Type") });
	const TArray<FName> Role = FindAliases(Columns, { TEXT("Role"), TEXT("Slot"), TEXT("AssetRole") });
	const bool bMissing = Match.IsEmpty() || Path.IsEmpty() || Kind.IsEmpty() || Role.IsEmpty();
	const bool bAmbiguous = Match.Num() > 1 || Path.Num() > 1 || Kind.Num() > 1 || Role.Num() > 1;
	if (!Match.IsEmpty()) Result.MatchColumn = Match[0];
	if (!Path.IsEmpty()) Result.AssetPathColumn = Path[0];
	if (!Kind.IsEmpty()) Result.AssetKindColumn = Kind[0];
	if (!Role.IsEmpty()) Result.RoleColumn = Role[0];

	if (bMissing)
	{
		Result.Disposition = EDataForgeInferenceDisposition::Unsupported;
		AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Error, TEXT("DF2040"),
			TEXT("Association Source must expose unambiguous Match, Asset Path, Asset Kind, and Role columns."), AdapterId);
	}
	else if (bAmbiguous)
	{
		Result.Disposition = EDataForgeInferenceDisposition::Ambiguous;
		AddDiagnostic(Result.Diagnostics, EDataForgeSeverity::Warning, TEXT("DF2041"),
			TEXT("Multiple columns match one or more Association Source semantics; explicit confirmation is required."), AdapterId);
	}
	else
	{
		const bool bCanonical = Result.MatchColumn == TEXT("Subject") && Result.AssetPathColumn == TEXT("ObjectPath")
			&& Result.AssetKindColumn == TEXT("AssetKind") && Result.RoleColumn == TEXT("Role");
		Result.Disposition = bCanonical ? EDataForgeInferenceDisposition::Exact : EDataForgeInferenceDisposition::Recommended;
	}
	AddSemanticEvidence(Result.Evidence, TEXT("MatchColumn"), Result.MatchColumn);
	AddSemanticEvidence(Result.Evidence, TEXT("AssetPathColumn"), Result.AssetPathColumn);
	AddSemanticEvidence(Result.Evidence, TEXT("AssetKindColumn"), Result.AssetKindColumn);
	AddSemanticEvidence(Result.Evidence, TEXT("RoleColumn"), Result.RoleColumn);
	return Result;
}
