#include "DataForgeAuthoringTypes.h"

#include "Misc/SecureHash.h"

namespace DataForgeAuthoringTypes
{
	void AppendField(FString& Target, const FString& Value)
	{
		Target += FString::Printf(TEXT("%d:%s"), Value.Len(), *Value);
	}

	void NormalizeFolder(FString& Folder)
	{
		Folder.ReplaceInline(TEXT("\\"), TEXT("/"));
		while (Folder.EndsWith(TEXT("/"))) Folder.LeftChopInline(1);
	}

	void NormalizeStrings(TArray<FString>& Values)
	{
		for (FString& Value : Values) Value.TrimStartAndEndInline();
		Values.RemoveAll([](const FString& Value) { return Value.IsEmpty(); });
		Values.Sort();
		for (int32 Index = Values.Num() - 1; Index > 0; --Index)
		{
			if (Values[Index] == Values[Index - 1]) Values.RemoveAt(Index);
		}
	}

	void AppendSourceConfig(FString& Target, const FDataForgeSourceConfig& Source)
	{
		AppendField(Target, Source.AdapterId.ToString());
		AppendField(Target, Source.File.FilePath);
		AppendField(Target, Source.SourceAsset.ToSoftObjectPath().ToString());
		AppendField(Target, FString::FromInt(Source.ProbeRowLimit));

		TArray<FName> ParameterNames;
		Source.Parameters.GetKeys(ParameterNames);
		ParameterNames.Sort(FNameLexicalLess());
		for (const FName Name : ParameterNames)
		{
			AppendField(Target, Name.ToString());
			AppendField(Target, Source.Parameters.FindChecked(Name));
		}

		for (const FDataForgeSourceInput& Input : Source.Inputs)
		{
			AppendField(Target, Input.AdapterId.ToString());
			AppendField(Target, Input.File.FilePath);
			AppendField(Target, Input.SourceAsset.ToSoftObjectPath().ToString());
			AppendField(Target, Input.JoinColumn.ToString());
			AppendField(Target, Input.ColumnPrefix);
			TArray<FName> InputParameterNames;
			Input.Parameters.GetKeys(InputParameterNames);
			InputParameterNames.Sort(FNameLexicalLess());
			for (const FName Name : InputParameterNames)
			{
				AppendField(Target, Name.ToString());
				AppendField(Target, Input.Parameters.FindChecked(Name));
			}
		}
	}
}

void FDataForgeAuthoringIntent::Normalize()
{
	for (FString& Root : AssetSearchRoots) DataForgeAuthoringTypes::NormalizeFolder(Root);
	DataForgeAuthoringTypes::NormalizeStrings(AssetSearchRoots);
	DataForgeAuthoringTypes::NormalizeFolder(DataTablePath);
	for (FDataForgeRequestedOutput& Output : Outputs)
	{
		DataForgeAuthoringTypes::NormalizeFolder(Output.OutputFolder);
		Output.RowReferenceProperty.TrimStartAndEndInline();
	}
	for (FDataForgeAssignmentHint& Hint : AssignmentHints) Hint.TargetProperty.TrimStartAndEndInline();

	Outputs.Sort([](const FDataForgeRequestedOutput& A, const FDataForgeRequestedOutput& B)
	{
		if (A.OutputName != B.OutputName) return A.OutputName.LexicalLess(B.OutputName);
		return A.AssetClass.ToSoftObjectPath().ToString() < B.AssetClass.ToSoftObjectPath().ToString();
	});
	AssignmentHints.Sort([](const FDataForgeAssignmentHint& A, const FDataForgeAssignmentHint& B)
	{
		if (A.OutputName != B.OutputName) return A.OutputName.LexicalLess(B.OutputName);
		if (A.AssetKind != B.AssetKind) return A.AssetKind.LexicalLess(B.AssetKind);
		if (A.Role != B.Role) return A.Role.LexicalLess(B.Role);
		return A.TargetProperty < B.TargetProperty;
	});
}

void FDataForgeAuthoringPlan::Normalize()
{
	Intent.Normalize();
	for (FDataForgeAuthoringDecision& Decision : Decisions)
	{
		DataForgeAuthoringTypes::NormalizeStrings(Decision.Alternatives);
		Decision.Evidence.Sort([](const FDataForgeInferenceEvidence& A, const FDataForgeInferenceEvidence& B)
		{
			if (A.Kind != B.Kind) return A.Kind.LexicalLess(B.Kind);
			if (A.Value != B.Value) return A.Value < B.Value;
			return A.Source < B.Source;
		});
	}
	Decisions.Sort([](const FDataForgeAuthoringDecision& A, const FDataForgeAuthoringDecision& B)
	{
		return A.DecisionId.LexicalLess(B.DecisionId);
	});
	Artifacts.Sort([](const FDataForgeAuthoringArtifact& A, const FDataForgeAuthoringArtifact& B)
	{
		if (A.ArtifactKind != B.ArtifactKind) return A.ArtifactKind.LexicalLess(B.ArtifactKind);
		if (A.ObjectPath != B.ObjectPath) return A.ObjectPath < B.ObjectPath;
		return static_cast<uint8>(A.Action) < static_cast<uint8>(B.Action);
	});
	Diagnostics.Sort([](const FDataForgeDiagnostic& A, const FDataForgeDiagnostic& B)
	{
		if (A.Severity != B.Severity) return static_cast<uint8>(A.Severity) < static_cast<uint8>(B.Severity);
		if (A.Code != B.Code) return A.Code < B.Code;
		if (A.RecordId != B.RecordId) return A.RecordId.LexicalLess(B.RecordId);
		if (A.Field != B.Field) return A.Field.LexicalLess(B.Field);
		if (A.SourceRow != B.SourceRow) return A.SourceRow < B.SourceRow;
		return A.Message < B.Message;
	});
}

bool FDataForgeAuthoringPlan::HasBlockingIssues() const
{
	if (Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Severity == EDataForgeSeverity::Error;
	}))
	{
		return true;
	}
	return Decisions.ContainsByPredicate([](const FDataForgeAuthoringDecision& Decision)
	{
		return Decision.bRequired
			&& Decision.Disposition != EDataForgeInferenceDisposition::Exact
			&& Decision.Disposition != EDataForgeInferenceDisposition::Recommended;
	});
}

FString FDataForgeAuthoringPlan::MakeStableSignature() const
{
	FDataForgeAuthoringPlan Normalized = *this;
	Normalized.Normalize();
	FString Canonical;
	DataForgeAuthoringTypes::AppendSourceConfig(Canonical, Normalized.Intent.Source);
	DataForgeAuthoringTypes::AppendField(Canonical, Normalized.Intent.PreferredPrimaryKey.ToString());
	for (const FString& Root : Normalized.Intent.AssetSearchRoots) DataForgeAuthoringTypes::AppendField(Canonical, Root);
	DataForgeAuthoringTypes::AppendField(Canonical,
		Normalized.Intent.RowStruct.IsValid() ? Normalized.Intent.RowStruct->GetPathName() : FString());
	DataForgeAuthoringTypes::AppendField(Canonical, Normalized.Intent.DataTablePath);
	for (const FDataForgeRequestedOutput& Output : Normalized.Intent.Outputs)
	{
		DataForgeAuthoringTypes::AppendField(Canonical, Output.OutputName.ToString());
		DataForgeAuthoringTypes::AppendField(Canonical, Output.AssetClass.ToSoftObjectPath().ToString());
		DataForgeAuthoringTypes::AppendField(Canonical, Output.OutputFolder);
		DataForgeAuthoringTypes::AppendField(Canonical, Output.RowReferenceProperty);
	}
	for (const FDataForgeAssignmentHint& Hint : Normalized.Intent.AssignmentHints)
	{
		DataForgeAuthoringTypes::AppendField(Canonical, Hint.AssetKind.ToString());
		DataForgeAuthoringTypes::AppendField(Canonical, Hint.Role.ToString());
		DataForgeAuthoringTypes::AppendField(Canonical, Hint.OutputName.ToString());
		DataForgeAuthoringTypes::AppendField(Canonical, Hint.TargetProperty);
	}
	for (const FDataForgeAuthoringDecision& Decision : Normalized.Decisions)
	{
		DataForgeAuthoringTypes::AppendField(Canonical, Decision.DecisionId.ToString());
		DataForgeAuthoringTypes::AppendField(Canonical, FString::FromInt(static_cast<uint8>(Decision.Disposition)));
		DataForgeAuthoringTypes::AppendField(Canonical, Decision.bRequired ? TEXT("1") : TEXT("0"));
		DataForgeAuthoringTypes::AppendField(Canonical, Decision.SelectedValue);
		for (const FString& Alternative : Decision.Alternatives) DataForgeAuthoringTypes::AppendField(Canonical, Alternative);
		for (const FDataForgeInferenceEvidence& Evidence : Decision.Evidence)
		{
			DataForgeAuthoringTypes::AppendField(Canonical, Evidence.Kind.ToString());
			DataForgeAuthoringTypes::AppendField(Canonical, Evidence.Value);
			DataForgeAuthoringTypes::AppendField(Canonical, Evidence.Source);
		}
	}
	for (const FDataForgeAuthoringArtifact& Artifact : Normalized.Artifacts)
	{
		DataForgeAuthoringTypes::AppendField(Canonical, Artifact.ArtifactKind.ToString());
		DataForgeAuthoringTypes::AppendField(Canonical, FString::FromInt(static_cast<uint8>(Artifact.Action)));
		DataForgeAuthoringTypes::AppendField(Canonical, Artifact.ObjectPath);
	}
	for (const FDataForgeDiagnostic& Diagnostic : Normalized.Diagnostics)
	{
		DataForgeAuthoringTypes::AppendField(Canonical, FString::FromInt(static_cast<uint8>(Diagnostic.Severity)));
		DataForgeAuthoringTypes::AppendField(Canonical, Diagnostic.Code);
		DataForgeAuthoringTypes::AppendField(Canonical, Diagnostic.RecordId.ToString());
		DataForgeAuthoringTypes::AppendField(Canonical, Diagnostic.Field.ToString());
		DataForgeAuthoringTypes::AppendField(Canonical, FString::FromInt(Diagnostic.SourceRow));
		DataForgeAuthoringTypes::AppendField(Canonical, Diagnostic.Message);
	}
	return FMD5::HashAnsiString(*Canonical);
}
