#include "DataForgeRuleSetSnapshot.h"

#include "DataForgeRuleSet.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonWriter.h"

namespace DataForgeRuleSetSnapshot
{
	constexpr int32 SnapshotVersion = 3;

	struct FParameter
	{
		FString Key;
		FString Value;
	};

	struct FSourceInput
	{
		FString AdapterId;
		FString File;
		FString Asset;
		FString JoinColumn;
		FString ColumnPrefix;
		TArray<FParameter> Parameters;
	};

	struct FAssetRule
	{
		FString RuleId;
		FString Ownership;
		FString BaseFolder;
		FString SubfolderPattern;
		FString AssetNamePattern;
	};

	struct FGeneratedOutput
	{
		FString OutputName;
		FString Type;
		FString AssetClass;
		FString AssetRuleId;
	};

	struct FBinding
	{
		FString Source;
		FString SourceColumn;
		FString SourceOutput;
		FString Target;
		FString TargetOutput;
		FString TargetProperty;
		FString AssetRuleId;
		bool bRequired = true;
	};

	struct FProfileRuleOrigin
	{
		FString GroupTemplateId;
		FString RuleTemplateId;
		FAssetRule Baseline;
		TArray<FString> OverrideFields;
	};

	struct FSnapshot
	{
		FString RuleSetPath;
		FString RuleSetId;
		int32 RuleVersion = 1;
		FString AdapterId;
		FString SourceFile;
		FString SourceAsset;
		int32 ProbeRowLimit = 20;
		TArray<FParameter> Parameters;
		TArray<FSourceInput> SourceInputs;
		FString PrimaryKey;
		TArray<FString> RequiredColumns;
		bool bWarnOnUnmappedColumns = true;
		FString RowStruct;
		FString AssetPath;
		bool bCreateIfMissing = true;
		bool bRemoveRowsMissingFromSource = false;
		bool bSaveAfterApply = true;
		FString ProfilePath;
		FString ProfileId;
		int32 MaterializedProfileVersion = 0;
		FString MaterializedProfileHash;
		TArray<FParameter> ProfileParameters;
		TArray<FProfileRuleOrigin> ProfileRules;
		TArray<FAssetRule> AssetRules;
		TArray<FGeneratedOutput> GeneratedOutputs;
		TArray<FBinding> Bindings;
		TArray<FString> Dependencies;
	};

	FString EnumName(const UEnum* Enum, int64 Value)
	{
		return Enum ? Enum->GetNameStringByValue(Value) : FString::FromInt(Value);
	}

	FSnapshot Build(const UDataForgeRuleSet& RuleSet)
	{
		FSnapshot Snapshot;
		Snapshot.RuleSetPath = RuleSet.GetPathName();
		Snapshot.RuleSetId = RuleSet.RuleSetId.ToString(EGuidFormats::DigitsWithHyphensLower);
		Snapshot.RuleVersion = RuleSet.RuleVersion;
		Snapshot.AdapterId = RuleSet.Source.AdapterId.ToString();
		Snapshot.SourceFile = RuleSet.Source.File.FilePath;
		Snapshot.SourceAsset = RuleSet.Source.SourceAsset.ToSoftObjectPath().ToString();
		Snapshot.ProbeRowLimit = RuleSet.Source.ProbeRowLimit;
		for (const TPair<FName, FString>& Pair : RuleSet.Source.Parameters)
		{
			Snapshot.Parameters.Add({ Pair.Key.ToString(), Pair.Value });
		}
		Snapshot.Parameters.Sort([](const FParameter& Left, const FParameter& Right)
		{
			return Left.Key < Right.Key;
		});
		for (const FDataForgeSourceInput& Input : RuleSet.Source.Inputs)
		{
			FSourceInput& SnapshotInput = Snapshot.SourceInputs.AddDefaulted_GetRef();
			SnapshotInput.AdapterId = Input.AdapterId.ToString();
			SnapshotInput.File = Input.File.FilePath;
			SnapshotInput.Asset = Input.SourceAsset.ToSoftObjectPath().ToString();
			SnapshotInput.JoinColumn = Input.JoinColumn.ToString();
			SnapshotInput.ColumnPrefix = Input.ColumnPrefix;
			for (const TPair<FName, FString>& Pair : Input.Parameters) SnapshotInput.Parameters.Add({ Pair.Key.ToString(), Pair.Value });
			SnapshotInput.Parameters.Sort([](const FParameter& Left, const FParameter& Right) { return Left.Key < Right.Key; });
		}

		Snapshot.PrimaryKey = RuleSet.Schema.PrimaryKey.ToString();
		for (const FName RequiredColumn : RuleSet.Schema.RequiredColumns)
		{
			Snapshot.RequiredColumns.Add(RequiredColumn.ToString());
		}
		Snapshot.RequiredColumns.Sort();
		Snapshot.bWarnOnUnmappedColumns = RuleSet.Schema.bWarnOnUnmappedColumns;

		Snapshot.RowStruct = RuleSet.Output.RowStruct ? RuleSet.Output.RowStruct->GetPathName() : TEXT("None");
		Snapshot.AssetPath = RuleSet.Output.AssetPath;
		Snapshot.bCreateIfMissing = RuleSet.Output.bCreateIfMissing;
		Snapshot.bRemoveRowsMissingFromSource = RuleSet.Output.bRemoveRowsMissingFromSource;
		Snapshot.bSaveAfterApply = RuleSet.Output.bSaveAfterApply;
		Snapshot.ProfilePath = RuleSet.ProfileOrigin.Profile.ToSoftObjectPath().ToString();
		Snapshot.ProfileId = RuleSet.ProfileOrigin.ProfileId.ToString(EGuidFormats::DigitsWithHyphensLower);
		Snapshot.MaterializedProfileVersion = RuleSet.ProfileOrigin.MaterializedVersion;
		Snapshot.MaterializedProfileHash = RuleSet.ProfileOrigin.MaterializedHash;
		for (const TPair<FName, FString>& Pair : RuleSet.ProfileOrigin.ParameterValues)
		{
			Snapshot.ProfileParameters.Add({ Pair.Key.ToString(), Pair.Value });
		}
		Snapshot.ProfileParameters.Sort([](const FParameter& Left, const FParameter& Right) { return Left.Key < Right.Key; });
		for (const FDataForgeMaterializedRuleOrigin& Origin : RuleSet.ProfileOrigin.Rules)
		{
			FProfileRuleOrigin& SnapshotOrigin = Snapshot.ProfileRules.AddDefaulted_GetRef();
			SnapshotOrigin.GroupTemplateId = Origin.GroupTemplateId.ToString(EGuidFormats::DigitsWithHyphensLower);
			SnapshotOrigin.RuleTemplateId = Origin.RuleTemplateId.ToString(EGuidFormats::DigitsWithHyphensLower);
			SnapshotOrigin.Baseline = {
				Origin.BaselineRule.RuleId.ToString(),
				EnumName(StaticEnum<EDataForgeAssetOwnership>(), static_cast<int64>(Origin.BaselineRule.Ownership)),
				Origin.BaselineRule.BaseFolder,
				Origin.BaselineRule.SubfolderPattern,
				Origin.BaselineRule.AssetNamePattern };
			const FDataForgeAssetRule* Current = RuleSet.AssetRules.FindByPredicate([&Origin](const FDataForgeAssetRule& Rule)
			{
				return Rule.RuleId == Origin.BaselineRule.RuleId;
			});
			if (!Current)
			{
				SnapshotOrigin.OverrideFields.Add(TEXT("Missing"));
			}
			else
			{
				if (Current->Ownership != Origin.BaselineRule.Ownership) SnapshotOrigin.OverrideFields.Add(TEXT("Ownership"));
				if (Current->BaseFolder != Origin.BaselineRule.BaseFolder) SnapshotOrigin.OverrideFields.Add(TEXT("BaseFolder"));
				if (Current->SubfolderPattern != Origin.BaselineRule.SubfolderPattern) SnapshotOrigin.OverrideFields.Add(TEXT("SubfolderPattern"));
				if (Current->AssetNamePattern != Origin.BaselineRule.AssetNamePattern) SnapshotOrigin.OverrideFields.Add(TEXT("AssetNamePattern"));
			}
		}
		Snapshot.ProfileRules.Sort([](const FProfileRuleOrigin& Left, const FProfileRuleOrigin& Right)
		{
			return Left.RuleTemplateId < Right.RuleTemplateId;
		});

		for (const FDataForgeAssetRule& Rule : RuleSet.AssetRules)
		{
			Snapshot.AssetRules.Add({
				Rule.RuleId.ToString(),
				EnumName(StaticEnum<EDataForgeAssetOwnership>(), static_cast<int64>(Rule.Ownership)),
				Rule.BaseFolder,
				Rule.SubfolderPattern,
				Rule.AssetNamePattern });
		}
		Snapshot.AssetRules.Sort([](const FAssetRule& Left, const FAssetRule& Right)
		{
			return Left.RuleId + TEXT("|") + Left.Ownership + TEXT("|") + Left.BaseFolder + TEXT("|") + Left.SubfolderPattern + TEXT("|") + Left.AssetNamePattern
				< Right.RuleId + TEXT("|") + Right.Ownership + TEXT("|") + Right.BaseFolder + TEXT("|") + Right.SubfolderPattern + TEXT("|") + Right.AssetNamePattern;
		});

		for (const FDataForgeGeneratedAssetOutputRule& Output : RuleSet.GeneratedOutputs)
		{
			Snapshot.GeneratedOutputs.Add({
				Output.OutputName.ToString(),
				EnumName(StaticEnum<EDataForgeGeneratedAssetType>(), static_cast<int64>(Output.Type)),
				Output.AssetClass ? Output.AssetClass->GetPathName() : TEXT("None"),
				Output.AssetRuleId.ToString() });
		}
		Snapshot.GeneratedOutputs.Sort([](const FGeneratedOutput& Left, const FGeneratedOutput& Right)
		{
			return Left.OutputName + TEXT("|") + Left.Type + TEXT("|") + Left.AssetClass + TEXT("|") + Left.AssetRuleId
				< Right.OutputName + TEXT("|") + Right.Type + TEXT("|") + Right.AssetClass + TEXT("|") + Right.AssetRuleId;
		});

		for (const FDataForgeBindingRule& Binding : RuleSet.Bindings)
		{
			Snapshot.Bindings.Add({
				EnumName(StaticEnum<EDataForgeBindingSource>(), static_cast<int64>(Binding.Source)),
				Binding.SourceColumn.ToString(),
				Binding.SourceOutput.ToString(),
				EnumName(StaticEnum<EDataForgeBindingTarget>(), static_cast<int64>(Binding.Target)),
				Binding.TargetOutput.ToString(),
				Binding.TargetProperty,
				Binding.AssetRuleId.ToString(),
				Binding.bRequired });
		}
		Snapshot.Bindings.Sort([](const FBinding& Left, const FBinding& Right)
		{
			const FString LeftKey = Left.Target + TEXT("|") + Left.TargetOutput + TEXT("|") + Left.TargetProperty
				+ TEXT("|") + Left.Source + TEXT("|") + Left.SourceColumn + TEXT("|") + Left.SourceOutput + TEXT("|") + Left.AssetRuleId
				+ (Left.bRequired ? TEXT("|1") : TEXT("|0"));
			const FString RightKey = Right.Target + TEXT("|") + Right.TargetOutput + TEXT("|") + Right.TargetProperty
				+ TEXT("|") + Right.Source + TEXT("|") + Right.SourceColumn + TEXT("|") + Right.SourceOutput + TEXT("|") + Right.AssetRuleId
				+ (Right.bRequired ? TEXT("|1") : TEXT("|0"));
			return LeftKey < RightKey;
		});

		for (const FDataForgeDependencyRule& Dependency : RuleSet.Dependencies)
		{
			Snapshot.Dependencies.Add(Dependency.RuleSet.ToSoftObjectPath().ToString());
		}
		Snapshot.Dependencies.Sort();
		return Snapshot;
	}

	FString JsonString(const FString& Value)
	{
		FString Result;
		Result.Reserve(Value.Len() + 2);
		Result.AppendChar(TEXT('"'));
		for (const TCHAR Character : Value)
		{
			switch (Character)
			{
			case TEXT('"'): Result += TEXT("\\\""); break;
			case TEXT('\\'): Result += TEXT("\\\\"); break;
			case TEXT('\b'): Result += TEXT("\\b"); break;
			case TEXT('\f'): Result += TEXT("\\f"); break;
			case TEXT('\n'): Result += TEXT("\\n"); break;
			case TEXT('\r'): Result += TEXT("\\r"); break;
			case TEXT('\t'): Result += TEXT("\\t"); break;
			default:
				if (Character < 0x20)
				{
					Result += FString::Printf(TEXT("\\u%04x"), static_cast<uint32>(Character));
				}
				else
				{
					Result.AppendChar(Character);
				}
				break;
			}
		}
		Result.AppendChar(TEXT('"'));
		return Result;
	}

	void AddYamlString(FString& Out, int32 Indent, const TCHAR* Key, const FString& Value)
	{
		Out += FString::ChrN(Indent, TEXT(' ')) + Key + TEXT(": ") + JsonString(Value) + TEXT("\n");
	}

	void AddYamlBool(FString& Out, int32 Indent, const TCHAR* Key, bool bValue)
	{
		Out += FString::ChrN(Indent, TEXT(' ')) + Key + (bValue ? TEXT(": true\n") : TEXT(": false\n"));
	}

	FString NormalizeNewlines(FString Text)
	{
		Text.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
		Text.ReplaceInline(TEXT("\r"), TEXT("\n"));
		return Text;
	}

	FDataForgeResult MakeResult(bool bSuccess, TArray<FDataForgeDiagnostic>&& Diagnostics, const FString& Summary)
	{
		FDataForgeResult Result;
		Result.bSuccess = bSuccess;
		Result.Diagnostics = MoveTemp(Diagnostics);
		Result.Summary = Summary;
		return Result;
	}

	void AddError(TArray<FDataForgeDiagnostic>& Diagnostics, const TCHAR* Code, const FString& Message)
	{
		FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = EDataForgeSeverity::Error;
		Diagnostic.Code = Code;
		Diagnostic.Message = Message;
	}
}

FString FDataForgeRuleSetSnapshot::SerializeJson(const UDataForgeRuleSet& RuleSet)
{
	using namespace DataForgeRuleSetSnapshot;
	const FSnapshot Snapshot = Build(RuleSet);
	FString Json;
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json);

	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("snapshotVersion"), SnapshotVersion);
	Writer->WriteValue(TEXT("ruleSetPath"), Snapshot.RuleSetPath);
	Writer->WriteValue(TEXT("ruleSetId"), Snapshot.RuleSetId);
	Writer->WriteValue(TEXT("ruleVersion"), Snapshot.RuleVersion);

	Writer->WriteObjectStart(TEXT("source"));
	Writer->WriteValue(TEXT("adapterId"), Snapshot.AdapterId);
	Writer->WriteValue(TEXT("file"), Snapshot.SourceFile);
	Writer->WriteValue(TEXT("asset"), Snapshot.SourceAsset);
	Writer->WriteValue(TEXT("probeRowLimit"), Snapshot.ProbeRowLimit);
	Writer->WriteObjectStart(TEXT("parameters"));
	for (const FParameter& Parameter : Snapshot.Parameters)
	{
		Writer->WriteValue(Parameter.Key, Parameter.Value);
	}
	Writer->WriteObjectEnd();
	Writer->WriteArrayStart(TEXT("inputs"));
	for (const FSourceInput& Input : Snapshot.SourceInputs)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("adapterId"), Input.AdapterId);
		Writer->WriteValue(TEXT("file"), Input.File);
		Writer->WriteValue(TEXT("asset"), Input.Asset);
		Writer->WriteValue(TEXT("joinColumn"), Input.JoinColumn);
		Writer->WriteValue(TEXT("columnPrefix"), Input.ColumnPrefix);
		Writer->WriteObjectStart(TEXT("parameters"));
		for (const FParameter& Parameter : Input.Parameters) Writer->WriteValue(Parameter.Key, Parameter.Value);
		Writer->WriteObjectEnd();
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();
	Writer->WriteObjectEnd();

	Writer->WriteObjectStart(TEXT("schema"));
	Writer->WriteValue(TEXT("primaryKey"), Snapshot.PrimaryKey);
	Writer->WriteArrayStart(TEXT("requiredColumns"));
	for (const FString& Column : Snapshot.RequiredColumns) Writer->WriteValue(Column);
	Writer->WriteArrayEnd();
	Writer->WriteValue(TEXT("warnOnUnmappedColumns"), Snapshot.bWarnOnUnmappedColumns);
	Writer->WriteObjectEnd();

	Writer->WriteObjectStart(TEXT("output"));
	Writer->WriteValue(TEXT("rowStruct"), Snapshot.RowStruct);
	Writer->WriteValue(TEXT("assetPath"), Snapshot.AssetPath);
	Writer->WriteValue(TEXT("createIfMissing"), Snapshot.bCreateIfMissing);
	Writer->WriteValue(TEXT("removeRowsMissingFromSource"), Snapshot.bRemoveRowsMissingFromSource);
	Writer->WriteValue(TEXT("saveAfterApply"), Snapshot.bSaveAfterApply);
	Writer->WriteObjectEnd();

	Writer->WriteObjectStart(TEXT("profileOrigin"));
	Writer->WriteValue(TEXT("profilePath"), Snapshot.ProfilePath);
	Writer->WriteValue(TEXT("profileId"), Snapshot.ProfileId);
	Writer->WriteValue(TEXT("materializedVersion"), Snapshot.MaterializedProfileVersion);
	Writer->WriteValue(TEXT("materializedHash"), Snapshot.MaterializedProfileHash);
	Writer->WriteObjectStart(TEXT("parameterValues"));
	for (const FParameter& Parameter : Snapshot.ProfileParameters) Writer->WriteValue(Parameter.Key, Parameter.Value);
	Writer->WriteObjectEnd();
	Writer->WriteArrayStart(TEXT("rules"));
	for (const FProfileRuleOrigin& Origin : Snapshot.ProfileRules)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("groupTemplateId"), Origin.GroupTemplateId);
		Writer->WriteValue(TEXT("ruleTemplateId"), Origin.RuleTemplateId);
		Writer->WriteObjectStart(TEXT("baseline"));
		Writer->WriteValue(TEXT("ruleId"), Origin.Baseline.RuleId);
		Writer->WriteValue(TEXT("ownership"), Origin.Baseline.Ownership);
		Writer->WriteValue(TEXT("baseFolder"), Origin.Baseline.BaseFolder);
		Writer->WriteValue(TEXT("subfolderPattern"), Origin.Baseline.SubfolderPattern);
		Writer->WriteValue(TEXT("assetNamePattern"), Origin.Baseline.AssetNamePattern);
		Writer->WriteObjectEnd();
		Writer->WriteArrayStart(TEXT("overrideFields"));
		for (const FString& Field : Origin.OverrideFields) Writer->WriteValue(Field);
		Writer->WriteArrayEnd();
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();
	Writer->WriteObjectEnd();

	Writer->WriteArrayStart(TEXT("assetRules"));
	for (const FAssetRule& Rule : Snapshot.AssetRules)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("ruleId"), Rule.RuleId);
		Writer->WriteValue(TEXT("ownership"), Rule.Ownership);
		Writer->WriteValue(TEXT("baseFolder"), Rule.BaseFolder);
		Writer->WriteValue(TEXT("subfolderPattern"), Rule.SubfolderPattern);
		Writer->WriteValue(TEXT("assetNamePattern"), Rule.AssetNamePattern);
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();

	Writer->WriteArrayStart(TEXT("generatedOutputs"));
	for (const FGeneratedOutput& Output : Snapshot.GeneratedOutputs)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("outputName"), Output.OutputName);
		Writer->WriteValue(TEXT("type"), Output.Type);
		Writer->WriteValue(TEXT("assetClass"), Output.AssetClass);
		Writer->WriteValue(TEXT("assetRuleId"), Output.AssetRuleId);
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();

	Writer->WriteArrayStart(TEXT("bindings"));
	for (const FBinding& Binding : Snapshot.Bindings)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("source"), Binding.Source);
		Writer->WriteValue(TEXT("sourceColumn"), Binding.SourceColumn);
		Writer->WriteValue(TEXT("sourceOutput"), Binding.SourceOutput);
		Writer->WriteValue(TEXT("target"), Binding.Target);
		Writer->WriteValue(TEXT("targetOutput"), Binding.TargetOutput);
		Writer->WriteValue(TEXT("targetProperty"), Binding.TargetProperty);
		Writer->WriteValue(TEXT("assetRuleId"), Binding.AssetRuleId);
		Writer->WriteValue(TEXT("required"), Binding.bRequired);
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();

	Writer->WriteArrayStart(TEXT("dependencies"));
	for (const FString& Dependency : Snapshot.Dependencies) Writer->WriteValue(Dependency);
	Writer->WriteArrayEnd();
	Writer->WriteObjectEnd();
	Writer->Close();
	return NormalizeNewlines(Json) + TEXT("\n");
}

FString FDataForgeRuleSetSnapshot::SerializeYaml(const UDataForgeRuleSet& RuleSet)
{
	using namespace DataForgeRuleSetSnapshot;
	const FSnapshot Snapshot = Build(RuleSet);
	FString Yaml = TEXT("# Generated by DataForge. Edit the RuleSet asset, then export again.\n");
	Yaml += FString::Printf(TEXT("snapshotVersion: %d\n"), SnapshotVersion);
	AddYamlString(Yaml, 0, TEXT("ruleSetPath"), Snapshot.RuleSetPath);
	AddYamlString(Yaml, 0, TEXT("ruleSetId"), Snapshot.RuleSetId);
	Yaml += FString::Printf(TEXT("ruleVersion: %d\n"), Snapshot.RuleVersion);

	Yaml += TEXT("source:\n");
	AddYamlString(Yaml, 2, TEXT("adapterId"), Snapshot.AdapterId);
	AddYamlString(Yaml, 2, TEXT("file"), Snapshot.SourceFile);
	AddYamlString(Yaml, 2, TEXT("asset"), Snapshot.SourceAsset);
	Yaml += FString::Printf(TEXT("  probeRowLimit: %d\n"), Snapshot.ProbeRowLimit);
	if (Snapshot.Parameters.IsEmpty())
	{
		Yaml += TEXT("  parameters: {}\n");
	}
	else
	{
		Yaml += TEXT("  parameters:\n");
		for (const FParameter& Parameter : Snapshot.Parameters)
		{
			Yaml += TEXT("    ") + JsonString(Parameter.Key) + TEXT(": ") + JsonString(Parameter.Value) + TEXT("\n");
		}
	}
	if (Snapshot.SourceInputs.IsEmpty())
	{
		Yaml += TEXT("  inputs: []\n");
	}
	else
	{
		Yaml += TEXT("  inputs:\n");
		for (const FSourceInput& Input : Snapshot.SourceInputs)
		{
			Yaml += TEXT("    - adapterId: ") + JsonString(Input.AdapterId) + TEXT("\n");
			Yaml += TEXT("      file: ") + JsonString(Input.File) + TEXT("\n");
			Yaml += TEXT("      asset: ") + JsonString(Input.Asset) + TEXT("\n");
			Yaml += TEXT("      joinColumn: ") + JsonString(Input.JoinColumn) + TEXT("\n");
			Yaml += TEXT("      columnPrefix: ") + JsonString(Input.ColumnPrefix) + TEXT("\n");
			if (Input.Parameters.IsEmpty())
			{
				Yaml += TEXT("      parameters: {}\n");
			}
			else
			{
				Yaml += TEXT("      parameters:\n");
				for (const FParameter& Parameter : Input.Parameters)
				{
					Yaml += TEXT("        ") + JsonString(Parameter.Key) + TEXT(": ") + JsonString(Parameter.Value) + TEXT("\n");
				}
			}
		}
	}

	Yaml += TEXT("schema:\n");
	AddYamlString(Yaml, 2, TEXT("primaryKey"), Snapshot.PrimaryKey);
	if (Snapshot.RequiredColumns.IsEmpty())
	{
		Yaml += TEXT("  requiredColumns: []\n");
	}
	else
	{
		Yaml += TEXT("  requiredColumns:\n");
		for (const FString& Column : Snapshot.RequiredColumns) Yaml += TEXT("    - ") + JsonString(Column) + TEXT("\n");
	}
	AddYamlBool(Yaml, 2, TEXT("warnOnUnmappedColumns"), Snapshot.bWarnOnUnmappedColumns);

	Yaml += TEXT("output:\n");
	AddYamlString(Yaml, 2, TEXT("rowStruct"), Snapshot.RowStruct);
	AddYamlString(Yaml, 2, TEXT("assetPath"), Snapshot.AssetPath);
	AddYamlBool(Yaml, 2, TEXT("createIfMissing"), Snapshot.bCreateIfMissing);
	AddYamlBool(Yaml, 2, TEXT("removeRowsMissingFromSource"), Snapshot.bRemoveRowsMissingFromSource);
	AddYamlBool(Yaml, 2, TEXT("saveAfterApply"), Snapshot.bSaveAfterApply);

	Yaml += TEXT("profileOrigin:\n");
	AddYamlString(Yaml, 2, TEXT("profilePath"), Snapshot.ProfilePath);
	AddYamlString(Yaml, 2, TEXT("profileId"), Snapshot.ProfileId);
	Yaml += FString::Printf(TEXT("  materializedVersion: %d\n"), Snapshot.MaterializedProfileVersion);
	AddYamlString(Yaml, 2, TEXT("materializedHash"), Snapshot.MaterializedProfileHash);
	if (Snapshot.ProfileParameters.IsEmpty())
	{
		Yaml += TEXT("  parameterValues: {}\n");
	}
	else
	{
		Yaml += TEXT("  parameterValues:\n");
		for (const FParameter& Parameter : Snapshot.ProfileParameters)
		{
			Yaml += TEXT("    ") + JsonString(Parameter.Key) + TEXT(": ") + JsonString(Parameter.Value) + TEXT("\n");
		}
	}
	if (Snapshot.ProfileRules.IsEmpty())
	{
		Yaml += TEXT("  rules: []\n");
	}
	else
	{
		Yaml += TEXT("  rules:\n");
		for (const FProfileRuleOrigin& Origin : Snapshot.ProfileRules)
		{
			Yaml += TEXT("    - groupTemplateId: ") + JsonString(Origin.GroupTemplateId) + TEXT("\n");
			AddYamlString(Yaml, 6, TEXT("ruleTemplateId"), Origin.RuleTemplateId);
			Yaml += TEXT("      baseline:\n");
			AddYamlString(Yaml, 8, TEXT("ruleId"), Origin.Baseline.RuleId);
			AddYamlString(Yaml, 8, TEXT("ownership"), Origin.Baseline.Ownership);
			AddYamlString(Yaml, 8, TEXT("baseFolder"), Origin.Baseline.BaseFolder);
			AddYamlString(Yaml, 8, TEXT("subfolderPattern"), Origin.Baseline.SubfolderPattern);
			AddYamlString(Yaml, 8, TEXT("assetNamePattern"), Origin.Baseline.AssetNamePattern);
			if (Origin.OverrideFields.IsEmpty())
			{
				Yaml += TEXT("      overrideFields: []\n");
			}
			else
			{
				Yaml += TEXT("      overrideFields:\n");
				for (const FString& Field : Origin.OverrideFields) Yaml += TEXT("        - ") + JsonString(Field) + TEXT("\n");
			}
		}
	}

	if (Snapshot.AssetRules.IsEmpty())
	{
		Yaml += TEXT("assetRules: []\n");
	}
	else
	{
		Yaml += TEXT("assetRules:\n");
		for (const FAssetRule& Rule : Snapshot.AssetRules)
		{
			Yaml += TEXT("  - ruleId: ") + JsonString(Rule.RuleId) + TEXT("\n");
			AddYamlString(Yaml, 4, TEXT("ownership"), Rule.Ownership);
			AddYamlString(Yaml, 4, TEXT("baseFolder"), Rule.BaseFolder);
			AddYamlString(Yaml, 4, TEXT("subfolderPattern"), Rule.SubfolderPattern);
			AddYamlString(Yaml, 4, TEXT("assetNamePattern"), Rule.AssetNamePattern);
		}
	}

	if (Snapshot.GeneratedOutputs.IsEmpty())
	{
		Yaml += TEXT("generatedOutputs: []\n");
	}
	else
	{
		Yaml += TEXT("generatedOutputs:\n");
		for (const FGeneratedOutput& Output : Snapshot.GeneratedOutputs)
		{
			Yaml += TEXT("  - outputName: ") + JsonString(Output.OutputName) + TEXT("\n");
			AddYamlString(Yaml, 4, TEXT("type"), Output.Type);
			AddYamlString(Yaml, 4, TEXT("assetClass"), Output.AssetClass);
			AddYamlString(Yaml, 4, TEXT("assetRuleId"), Output.AssetRuleId);
		}
	}

	if (Snapshot.Bindings.IsEmpty())
	{
		Yaml += TEXT("bindings: []\n");
	}
	else
	{
		Yaml += TEXT("bindings:\n");
		for (const FBinding& Binding : Snapshot.Bindings)
		{
			Yaml += TEXT("  - source: ") + JsonString(Binding.Source) + TEXT("\n");
			AddYamlString(Yaml, 4, TEXT("sourceColumn"), Binding.SourceColumn);
			AddYamlString(Yaml, 4, TEXT("sourceOutput"), Binding.SourceOutput);
			AddYamlString(Yaml, 4, TEXT("target"), Binding.Target);
			AddYamlString(Yaml, 4, TEXT("targetOutput"), Binding.TargetOutput);
			AddYamlString(Yaml, 4, TEXT("targetProperty"), Binding.TargetProperty);
			AddYamlString(Yaml, 4, TEXT("assetRuleId"), Binding.AssetRuleId);
			AddYamlBool(Yaml, 4, TEXT("required"), Binding.bRequired);
		}
	}

	if (Snapshot.Dependencies.IsEmpty())
	{
		Yaml += TEXT("dependencies: []\n");
	}
	else
	{
		Yaml += TEXT("dependencies:\n");
		for (const FString& Dependency : Snapshot.Dependencies) Yaml += TEXT("  - ") + JsonString(Dependency) + TEXT("\n");
	}
	return Yaml;
}

void FDataForgeRuleSetSnapshot::GetFilenames(
	const UDataForgeRuleSet& RuleSet,
	const FString& RootDirectory,
	FString& OutJsonFilename,
	FString& OutYamlFilename)
{
	FString RelativePackageName = RuleSet.GetOutermost()->GetName();
	RelativePackageName.RemoveFromStart(TEXT("/"));
	const FString Root = RootDirectory.IsEmpty()
		? FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("DataForge"), TEXT("Snapshots"))
		: RootDirectory;
	const FString BaseFilename = FPaths::Combine(Root, RelativePackageName);
	OutJsonFilename = BaseFilename + TEXT(".json");
	OutYamlFilename = BaseFilename + TEXT(".yaml");
}

FDataForgeResult FDataForgeRuleSetSnapshot::Export(
	const UDataForgeRuleSet& RuleSet,
	const FString& RootDirectory,
	FString* OutJsonFilename,
	FString* OutYamlFilename)
{
	using namespace DataForgeRuleSetSnapshot;
	FString JsonFilename;
	FString YamlFilename;
	GetFilenames(RuleSet, RootDirectory, JsonFilename, YamlFilename);
	TArray<FDataForgeDiagnostic> Diagnostics;
	if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(JsonFilename), true))
	{
		AddError(Diagnostics, TEXT("DF5001"), FString::Printf(TEXT("Could not create snapshot directory: %s"), *FPaths::GetPath(JsonFilename)));
		return MakeResult(false, MoveTemp(Diagnostics), TEXT("RuleSet snapshot export failed."));
	}

	const bool bJsonSaved = FFileHelper::SaveStringToFile(
		SerializeJson(RuleSet),
		*JsonFilename,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	const bool bYamlSaved = FFileHelper::SaveStringToFile(
		SerializeYaml(RuleSet),
		*YamlFilename,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (!bJsonSaved) AddError(Diagnostics, TEXT("DF5002"), FString::Printf(TEXT("Could not write JSON snapshot: %s"), *JsonFilename));
	if (!bYamlSaved) AddError(Diagnostics, TEXT("DF5003"), FString::Printf(TEXT("Could not write YAML snapshot: %s"), *YamlFilename));
	if (OutJsonFilename) *OutJsonFilename = JsonFilename;
	if (OutYamlFilename) *OutYamlFilename = YamlFilename;
	const bool bSuccess = Diagnostics.IsEmpty();
	return MakeResult(
		bSuccess,
		MoveTemp(Diagnostics),
		bSuccess ? FString::Printf(TEXT("Exported RuleSet snapshots: %s and %s"), *JsonFilename, *YamlFilename) : TEXT("RuleSet snapshot export failed."));
}

FDataForgeResult FDataForgeRuleSetSnapshot::Verify(
	const UDataForgeRuleSet& RuleSet,
	const FString& RootDirectory)
{
	using namespace DataForgeRuleSetSnapshot;
	FString JsonFilename;
	FString YamlFilename;
	GetFilenames(RuleSet, RootDirectory, JsonFilename, YamlFilename);
	TArray<FDataForgeDiagnostic> Diagnostics;
	FString ExistingJson;
	FString ExistingYaml;
	if (!FFileHelper::LoadFileToString(ExistingJson, *JsonFilename))
	{
		AddError(Diagnostics, TEXT("DF5004"), FString::Printf(TEXT("JSON snapshot is missing or unreadable: %s"), *JsonFilename));
	}
	else if (NormalizeNewlines(ExistingJson) != SerializeJson(RuleSet))
	{
		AddError(Diagnostics, TEXT("DF5005"), FString::Printf(TEXT("JSON snapshot is stale: %s"), *JsonFilename));
	}
	if (!FFileHelper::LoadFileToString(ExistingYaml, *YamlFilename))
	{
		AddError(Diagnostics, TEXT("DF5006"), FString::Printf(TEXT("YAML snapshot is missing or unreadable: %s"), *YamlFilename));
	}
	else if (NormalizeNewlines(ExistingYaml) != SerializeYaml(RuleSet))
	{
		AddError(Diagnostics, TEXT("DF5007"), FString::Printf(TEXT("YAML snapshot is stale: %s"), *YamlFilename));
	}
	const bool bSuccess = Diagnostics.IsEmpty();
	return MakeResult(
		bSuccess,
		MoveTemp(Diagnostics),
		bSuccess ? TEXT("RuleSet JSON/YAML snapshots are current.") : TEXT("RuleSet snapshot verification failed."));
}
