#include "DataForgeAssetLayoutProfile.h"

#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"

namespace
{
	void AddError(TArray<FDataForgeDiagnostic>& Diagnostics, const TCHAR* Code, const FString& Message, FName Field = NAME_None)
	{
		FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = EDataForgeSeverity::Error;
		Diagnostic.Code = Code;
		Diagnostic.Message = Message;
		Diagnostic.Field = Field;
	}

	void AppendHashField(FString& Canonical, const FString& Value)
	{
		Canonical += FString::Printf(TEXT("%d:%s|"), Value.Len(), *Value);
	}

	FString GuidKey(const FGuid& Guid)
	{
		return Guid.ToString(EGuidFormats::Digits);
	}

	bool SubstituteParameters(
		const FString& Pattern,
		const TMap<FName, FString>& Values,
		FString& OutValue,
		TArray<FDataForgeDiagnostic>& Diagnostics,
		FName Field)
	{
		OutValue.Reset();
		int32 Cursor = 0;
		while (Cursor < Pattern.Len())
		{
			const int32 Open = Pattern.Find(TEXT("${"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Cursor);
			if (Open == INDEX_NONE)
			{
				OutValue += Pattern.Mid(Cursor);
				break;
			}

			OutValue += Pattern.Mid(Cursor, Open - Cursor);
			const int32 Close = Pattern.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open + 2);
			if (Close == INDEX_NONE)
			{
				AddError(Diagnostics, TEXT("DF1606"), FString::Printf(TEXT("Incomplete profile parameter token in '%s'."), *Pattern), Field);
				return false;
			}

			const FString ParameterName = Pattern.Mid(Open + 2, Close - Open - 2);
			if (ParameterName.IsEmpty())
			{
				AddError(Diagnostics, TEXT("DF1606"), TEXT("Profile parameter tokens cannot be empty."), Field);
				return false;
			}

			const FString* Value = Values.Find(FName(*ParameterName));
			if (!Value)
			{
				AddError(Diagnostics, TEXT("DF1605"), FString::Printf(TEXT("Unknown profile parameter '${%s}'."), *ParameterName), Field);
				return false;
			}

			OutValue += *Value;
			Cursor = Close + 1;
		}
		return true;
	}

	bool ValidateSourceTokens(
		const FString& Pattern,
		const TArray<FName>* SourceColumns,
		bool bAllowSourceTokens,
		TArray<FDataForgeDiagnostic>& Diagnostics,
		FName Field)
	{
		int32 Cursor = 0;
		while (Cursor < Pattern.Len())
		{
			const int32 Open = Pattern.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Cursor);
			if (Open == INDEX_NONE)
			{
				return true;
			}
			const int32 Close = Pattern.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open + 1);
			if (Close == INDEX_NONE)
			{
				AddError(Diagnostics, TEXT("DF1607"), FString::Printf(TEXT("Incomplete source column token in '%s'."), *Pattern), Field);
				return false;
			}

			const FString Column = Pattern.Mid(Open + 1, Close - Open - 1);
			if (!bAllowSourceTokens)
			{
				AddError(Diagnostics, TEXT("DF1608"), FString::Printf(TEXT("Source column token '{%s}' is not allowed in a layout root."), *Column), Field);
				return false;
			}
			if (Column.IsEmpty())
			{
				AddError(Diagnostics, TEXT("DF1607"), TEXT("Source column tokens cannot be empty."), Field);
				return false;
			}
			if (SourceColumns && !SourceColumns->ContainsByPredicate([&Column](const FName Candidate)
			{
				return Candidate.ToString() == Column;
			}))
			{
				AddError(Diagnostics, TEXT("DF1609"), FString::Printf(TEXT("Source column token '{%s}' does not match an available column exactly."), *Column), Field);
				return false;
			}
			Cursor = Close + 1;
		}
		return true;
	}

	FString JoinFolderPatterns(const FString& Left, const FString& Right)
	{
		FString NormalizedLeft = Left;
		FString NormalizedRight = Right;
		NormalizedLeft.ReplaceInline(TEXT("\\"), TEXT("/"));
		NormalizedRight.ReplaceInline(TEXT("\\"), TEXT("/"));
		NormalizedLeft.RemoveFromStart(TEXT("/"));
		NormalizedLeft.RemoveFromEnd(TEXT("/"));
		NormalizedRight.RemoveFromStart(TEXT("/"));
		NormalizedRight.RemoveFromEnd(TEXT("/"));
		if (NormalizedLeft.IsEmpty()) return NormalizedRight;
		if (NormalizedRight.IsEmpty()) return NormalizedLeft;
		return NormalizedLeft + TEXT("/") + NormalizedRight;
	}
}

FString FDataForgeAssetLayoutMaterializer::ComputeLayoutRevisionHash(const UDataForgeAssetLayoutProfile& Profile)
{
	FString Canonical;

	TArray<const FDataForgeLayoutRoot*> Roots;
	for (const FDataForgeLayoutRoot& Root : Profile.Roots) Roots.Add(&Root);
	Roots.Sort([](const FDataForgeLayoutRoot& A, const FDataForgeLayoutRoot& B)
	{
		return A.RootId.ToString() < B.RootId.ToString();
	});
	for (const FDataForgeLayoutRoot* Root : Roots)
	{
		AppendHashField(Canonical, Root->RootId.ToString());
		AppendHashField(Canonical, Root->PathPattern);
	}

	TArray<const FDataForgeProfileParameter*> Parameters;
	for (const FDataForgeProfileParameter& Parameter : Profile.Parameters) Parameters.Add(&Parameter);
	Parameters.Sort([](const FDataForgeProfileParameter& A, const FDataForgeProfileParameter& B)
	{
		return A.Name.ToString() < B.Name.ToString();
	});
	for (const FDataForgeProfileParameter* Parameter : Parameters)
	{
		AppendHashField(Canonical, Parameter->Name.ToString());
		AppendHashField(Canonical, FString::FromInt(static_cast<uint8>(Parameter->Type)));
		AppendHashField(Canonical, Parameter->DefaultValue);
		AppendHashField(Canonical, Parameter->bRequired ? TEXT("1") : TEXT("0"));
	}

	TArray<const FDataForgeAssetRuleGroupTemplate*> Groups;
	for (const FDataForgeAssetRuleGroupTemplate& Group : Profile.Groups) Groups.Add(&Group);
	Groups.Sort([](const FDataForgeAssetRuleGroupTemplate& A, const FDataForgeAssetRuleGroupTemplate& B)
	{
		return GuidKey(A.TemplateId) < GuidKey(B.TemplateId);
	});
	for (const FDataForgeAssetRuleGroupTemplate* Group : Groups)
	{
		AppendHashField(Canonical, GuidKey(Group->TemplateId));
		AppendHashField(Canonical, Group->GroupId.ToString());
		AppendHashField(Canonical, Group->RootId.ToString());
		AppendHashField(Canonical, Group->GroupSubfolderPattern);

		TArray<const FDataForgeAssetRuleTemplate*> Rules;
		for (const FDataForgeAssetRuleTemplate& Rule : Group->Rules) Rules.Add(&Rule);
		Rules.Sort([](const FDataForgeAssetRuleTemplate& A, const FDataForgeAssetRuleTemplate& B)
		{
			return GuidKey(A.TemplateId) < GuidKey(B.TemplateId);
		});
		for (const FDataForgeAssetRuleTemplate* Rule : Rules)
		{
			AppendHashField(Canonical, GuidKey(Rule->TemplateId));
			AppendHashField(Canonical, Rule->RuleId.ToString());
			AppendHashField(Canonical, FString::FromInt(static_cast<uint8>(Rule->Ownership)));
			AppendHashField(Canonical, Rule->RelativeFolderPattern);
			AppendHashField(Canonical, Rule->AssetNamePattern);
		}
	}

	FTCHARToUTF8 Utf8(*Canonical);
	FMD5 Md5;
	Md5.Update(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	uint8 Digest[16];
	Md5.Final(Digest);
	return BytesToHex(Digest, UE_ARRAY_COUNT(Digest));
}

FDataForgeResult FDataForgeAssetLayoutMaterializer::ValidateDefinition(const UDataForgeAssetLayoutProfile& Profile)
{
	TMap<FName, FString> ValidationValues;
	for (const FDataForgeProfileParameter& Parameter : Profile.Parameters)
	{
		FString Value = Parameter.DefaultValue;
		if (Value.IsEmpty() && Parameter.bRequired)
		{
			Value = Parameter.Type == EDataForgeProfileParameterType::ContentPath
				? TEXT("/Game/DataForgeValidation")
				: TEXT("Validation");
		}
		ValidationValues.Add(Parameter.Name, MoveTemp(Value));
	}
	const FDataForgeAssetLayoutMaterialization Materialized = Materialize(Profile, ValidationValues, nullptr);
	FDataForgeResult Result;
	Result.bSuccess = Materialized.bSuccess;
	Result.Diagnostics = Materialized.Diagnostics;
	Result.Summary = Materialized.bSuccess
		? FString::Printf(TEXT("Asset Layout Profile definition is valid (%d rule templates)."), Materialized.AssetRules.Num())
		: TEXT("Asset Layout Profile definition is invalid.");
	return Result;
}

FDataForgeAssetLayoutMaterialization FDataForgeAssetLayoutMaterializer::Materialize(
	const UDataForgeAssetLayoutProfile& Profile,
	const TMap<FName, FString>& ParameterValues,
	const TArray<FName>* SourceColumns)
{
	FDataForgeAssetLayoutMaterialization Result;
	if (!Profile.ProfileId.IsValid())
	{
		AddError(Result.Diagnostics, TEXT("DF1601"), TEXT("Asset Layout Profile has no valid Profile Id."));
	}
	if (Profile.ProfileVersion < 1)
	{
		AddError(Result.Diagnostics, TEXT("DF1602"), TEXT("Asset Layout Profile version must be at least 1."));
	}

	TSet<FName> ParameterNames;
	TMap<FName, FString> ResolvedParameters;
	for (const FDataForgeProfileParameter& Parameter : Profile.Parameters)
	{
		if (Parameter.Name.IsNone() || ParameterNames.Contains(Parameter.Name))
		{
			AddError(Result.Diagnostics, TEXT("DF1603"), TEXT("Profile parameter names must be non-empty and unique."), Parameter.Name);
			continue;
		}
		ParameterNames.Add(Parameter.Name);
		const FString* Supplied = ParameterValues.Find(Parameter.Name);
		const FString Value = Supplied ? *Supplied : Parameter.DefaultValue;
		if (Parameter.bRequired && Value.IsEmpty())
		{
			AddError(Result.Diagnostics, TEXT("DF1604"), FString::Printf(TEXT("Required profile parameter '%s' has no value."), *Parameter.Name.ToString()), Parameter.Name);
		}
		if (Parameter.Type == EDataForgeProfileParameterType::Name && Value.IsEmpty())
		{
			AddError(Result.Diagnostics, TEXT("DF1610"), FString::Printf(TEXT("Name parameter '%s' cannot be empty."), *Parameter.Name.ToString()), Parameter.Name);
		}
		if (Parameter.Type == EDataForgeProfileParameterType::ContentPath && !Value.IsEmpty() && !FPackageName::IsValidLongPackageName(Value))
		{
			AddError(Result.Diagnostics, TEXT("DF1611"), FString::Printf(TEXT("ContentPath parameter '%s' is not a valid long package path: %s"), *Parameter.Name.ToString(), *Value), Parameter.Name);
		}
		ResolvedParameters.Add(Parameter.Name, Value);
	}
	for (const TPair<FName, FString>& Supplied : ParameterValues)
	{
		if (!ParameterNames.Contains(Supplied.Key))
		{
			AddError(Result.Diagnostics, TEXT("DF1605"), FString::Printf(TEXT("Unknown supplied profile parameter '%s'."), *Supplied.Key.ToString()), Supplied.Key);
		}
	}

	TSet<FName> RootIds;
	TMap<FName, FString> ResolvedRoots;
	for (const FDataForgeLayoutRoot& Root : Profile.Roots)
	{
		if (Root.RootId.IsNone() || RootIds.Contains(Root.RootId))
		{
			AddError(Result.Diagnostics, TEXT("DF1612"), TEXT("Layout root ids must be non-empty and unique."), Root.RootId);
			continue;
		}
		RootIds.Add(Root.RootId);
		FString Resolved;
		if (SubstituteParameters(Root.PathPattern, ResolvedParameters, Resolved, Result.Diagnostics, Root.RootId)
			&& ValidateSourceTokens(Resolved, SourceColumns, false, Result.Diagnostics, Root.RootId))
		{
			Resolved.ReplaceInline(TEXT("\\"), TEXT("/"));
			Resolved.RemoveFromEnd(TEXT("/"));
			if (!FPackageName::IsValidLongPackageName(Resolved))
			{
				AddError(Result.Diagnostics, TEXT("DF1613"), FString::Printf(TEXT("Layout root '%s' does not resolve to a valid content path: %s"), *Root.RootId.ToString(), *Resolved), Root.RootId);
			}
			else
			{
				ResolvedRoots.Add(Root.RootId, Resolved);
			}
		}
	}

	TSet<FName> GroupIds;
	TSet<FGuid> GroupTemplateIds;
	TSet<FName> RuleIds;
	TSet<FGuid> RuleTemplateIds;
	for (const FDataForgeAssetRuleGroupTemplate& Group : Profile.Groups)
	{
		if (Group.GroupId.IsNone() || GroupIds.Contains(Group.GroupId))
		{
			AddError(Result.Diagnostics, TEXT("DF1614"), TEXT("Asset rule group ids must be non-empty and unique."), Group.GroupId);
		}
		else GroupIds.Add(Group.GroupId);
		if (!Group.TemplateId.IsValid() || GroupTemplateIds.Contains(Group.TemplateId))
		{
			AddError(Result.Diagnostics, TEXT("DF1615"), TEXT("Asset rule group Template Ids must be valid and unique."), Group.GroupId);
		}
		else GroupTemplateIds.Add(Group.TemplateId);

		const FString* RootPath = ResolvedRoots.Find(Group.RootId);
		if (!RootPath)
		{
			AddError(Result.Diagnostics, TEXT("DF1616"), FString::Printf(TEXT("Asset rule group '%s' references unknown or invalid root '%s'."), *Group.GroupId.ToString(), *Group.RootId.ToString()), Group.GroupId);
		}

		FString ResolvedGroupFolder;
		const bool bGroupFolderValid = SubstituteParameters(Group.GroupSubfolderPattern, ResolvedParameters, ResolvedGroupFolder, Result.Diagnostics, Group.GroupId)
			&& ValidateSourceTokens(ResolvedGroupFolder, SourceColumns, true, Result.Diagnostics, Group.GroupId);

		for (const FDataForgeAssetRuleTemplate& RuleTemplate : Group.Rules)
		{
			if (RuleTemplate.RuleId.IsNone() || RuleIds.Contains(RuleTemplate.RuleId))
			{
				AddError(Result.Diagnostics, TEXT("DF1617"), TEXT("Asset Rule Ids must be non-empty and unique across the Profile."), RuleTemplate.RuleId);
			}
			else RuleIds.Add(RuleTemplate.RuleId);
			if (!RuleTemplate.TemplateId.IsValid() || RuleTemplateIds.Contains(RuleTemplate.TemplateId))
			{
				AddError(Result.Diagnostics, TEXT("DF1618"), TEXT("Asset Rule Template Ids must be valid and unique across the Profile."), RuleTemplate.RuleId);
			}
			else RuleTemplateIds.Add(RuleTemplate.TemplateId);

			FString ResolvedFolder;
			FString ResolvedName;
			const bool bFolderValid = SubstituteParameters(RuleTemplate.RelativeFolderPattern, ResolvedParameters, ResolvedFolder, Result.Diagnostics, RuleTemplate.RuleId)
				&& ValidateSourceTokens(ResolvedFolder, SourceColumns, true, Result.Diagnostics, RuleTemplate.RuleId);
			const bool bNameValid = SubstituteParameters(RuleTemplate.AssetNamePattern, ResolvedParameters, ResolvedName, Result.Diagnostics, RuleTemplate.RuleId)
				&& ValidateSourceTokens(ResolvedName, SourceColumns, true, Result.Diagnostics, RuleTemplate.RuleId);
			if (ResolvedName.IsEmpty())
			{
				AddError(Result.Diagnostics, TEXT("DF1619"), TEXT("Asset name pattern cannot be empty."), RuleTemplate.RuleId);
			}

			if (RootPath && bGroupFolderValid && bFolderValid && bNameValid && !ResolvedName.IsEmpty())
			{
				FDataForgeAssetRule& Rule = Result.AssetRules.AddDefaulted_GetRef();
				Rule.RuleId = RuleTemplate.RuleId;
				Rule.Ownership = RuleTemplate.Ownership;
				Rule.BaseFolder = *RootPath;
				Rule.SubfolderPattern = JoinFolderPatterns(ResolvedGroupFolder, ResolvedFolder);
				Rule.AssetNamePattern = ResolvedName;

				FDataForgeMaterializedRuleOrigin& Origin = Result.Origin.Rules.AddDefaulted_GetRef();
				Origin.GroupTemplateId = Group.TemplateId;
				Origin.RuleTemplateId = RuleTemplate.TemplateId;
				Origin.BaselineRule = Rule;
			}
		}
	}

	Result.bSuccess = !Result.Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
	{
		return Diagnostic.Severity == EDataForgeSeverity::Error;
	});
	if (!Result.bSuccess)
	{
		Result.AssetRules.Reset();
		Result.Origin = FDataForgeProfileOrigin();
		return Result;
	}

	Result.Origin.Profile = const_cast<UDataForgeAssetLayoutProfile*>(&Profile);
	Result.Origin.ProfileId = Profile.ProfileId;
	Result.Origin.MaterializedVersion = Profile.ProfileVersion;
	Result.Origin.MaterializedHash = ComputeLayoutRevisionHash(Profile);
	Result.Origin.ParameterValues = MoveTemp(ResolvedParameters);
	return Result;
}
