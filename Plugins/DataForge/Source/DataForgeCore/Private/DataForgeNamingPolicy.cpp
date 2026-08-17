#include "DataForgeNamingPolicy.h"

#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"

namespace DataForgeNamingPolicy
{
	void AddError(TArray<FDataForgeDiagnostic>& Diagnostics, const TCHAR* Code, const FString& Message)
	{
		FDataForgeDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = EDataForgeSeverity::Error;
		Diagnostic.Code = Code;
		Diagnostic.Message = Message;
	}

	bool IsPascalComponent(const FString& Value, bool bAllowEmpty)
	{
		if (Value.IsEmpty()) return bAllowEmpty;
		if (!FChar::IsUpper(Value[0])) return false;
		for (const TCHAR Character : Value)
		{
			if (!FChar::IsAlnum(Character)) return false;
		}
		return true;
	}

	bool IsIdentifier(const FString& Value)
	{
		if (Value.IsEmpty()) return false;
		for (const TCHAR Character : Value)
		{
			if (!FChar::IsAlnum(Character)) return false;
		}
		return true;
	}

	bool IsDigits(const FString& Value)
	{
		if (Value.IsEmpty()) return false;
		for (const TCHAR Character : Value)
		{
			if (!FChar::IsDigit(Character)) return false;
		}
		return true;
	}

	const FDataForgeAssetKindNamingRule* FindKindById(const UDataForgeNamingPolicy& Policy, FName AssetKind)
	{
		return Policy.AssetKinds.FindByPredicate([AssetKind](const FDataForgeAssetKindNamingRule& Rule)
		{
			return Rule.AssetKind == AssetKind;
		});
	}

	const FDataForgeAssetKindNamingRule* FindKindByPrefix(const UDataForgeNamingPolicy& Policy, const FString& TypePrefix)
	{
		return Policy.AssetKinds.FindByPredicate([&TypePrefix](const FDataForgeAssetKindNamingRule& Rule)
		{
			return Rule.TypePrefix == TypePrefix;
		});
	}

	void ValidateComponent(const TCHAR* Label, const FString& Value, bool bAllowEmpty, TArray<FDataForgeDiagnostic>& Diagnostics)
	{
		if (!IsPascalComponent(Value, bAllowEmpty))
		{
			AddError(Diagnostics, TEXT("DF1710"), FString::Printf(TEXT("%s '%s' must be a PascalCase alphanumeric component."), Label, *Value));
		}
	}

	bool HasErrors(const TArray<FDataForgeDiagnostic>& Diagnostics)
	{
		return Diagnostics.ContainsByPredicate([](const FDataForgeDiagnostic& Diagnostic)
		{
			return Diagnostic.Severity == EDataForgeSeverity::Error;
		});
	}
}

void FDataForgeNamingPolicyResolver::ConfigureChimeraDefaults(UDataForgeNamingPolicy& Policy)
{
	Policy.ProjectPrefix = TEXT("CM");
	Policy.AssetKinds.Reset();

	auto AddKind = [&Policy](const TCHAR* AssetKind, const TCHAR* TypePrefix, const TCHAR* FolderName, UClass* ExpectedClass = nullptr)
	{
		FDataForgeAssetKindNamingRule& Rule = Policy.AssetKinds.AddDefaulted_GetRef();
		Rule.AssetKind = AssetKind;
		Rule.TypePrefix = TypePrefix;
		Rule.FolderName = FolderName;
		Rule.ExpectedAssetClass = ExpectedClass;
	};
	AddKind(TEXT("Blueprint"), TEXT("BP"), TEXT("Blueprint"), UBlueprint::StaticClass());
	AddKind(TEXT("WidgetBlueprint"), TEXT("WBP"), TEXT("Blueprint"));
	AddKind(TEXT("Texture"), TEXT("T"), TEXT("Texture"), UTexture::StaticClass());
	AddKind(TEXT("Material"), TEXT("M"), TEXT("Material"), UMaterialInterface::StaticClass());
	AddKind(TEXT("MaterialInstance"), TEXT("MI"), TEXT("Material"), UMaterialInterface::StaticClass());
	AddKind(TEXT("MaterialFunction"), TEXT("MF"), TEXT("Material"));
	AddKind(TEXT("StaticMesh"), TEXT("SM"), TEXT("Mesh"), UStaticMesh::StaticClass());
	AddKind(TEXT("SkeletalMesh"), TEXT("SK"), TEXT("Mesh"), USkeletalMesh::StaticClass());
	AddKind(TEXT("AnimationBlueprint"), TEXT("ABP"), TEXT("Animation"));
	AddKind(TEXT("AnimationSequence"), TEXT("A"), TEXT("Animation"));
	AddKind(TEXT("Skeleton"), TEXT("SKEL"), TEXT("Animation"));
	AddKind(TEXT("PhysicsAsset"), TEXT("PHYS"), TEXT("Physics"));
	AddKind(TEXT("NiagaraSystem"), TEXT("NS"), TEXT("Niagara"));
	AddKind(TEXT("NiagaraEmitter"), TEXT("NE"), TEXT("Niagara"));
	AddKind(TEXT("ParticleSystem"), TEXT("PS"), TEXT("Particle"));
	AddKind(TEXT("SoundWave"), TEXT("S"), TEXT("Audio"));
	AddKind(TEXT("SoundCue"), TEXT("SC"), TEXT("Audio"));
	AddKind(TEXT("DataTable"), TEXT("DT"), TEXT("Data"));
	AddKind(TEXT("CurveTable"), TEXT("CT"), TEXT("Data"));
	AddKind(TEXT("CurveFloat"), TEXT("CF"), TEXT("Data"));
	AddKind(TEXT("DataAsset"), TEXT("DA"), TEXT("Data"), UDataAsset::StaticClass());
	AddKind(TEXT("PrimaryDataAsset"), TEXT("PDA"), TEXT("Data"), UPrimaryDataAsset::StaticClass());
	AddKind(TEXT("LevelSequence"), TEXT("LS"), TEXT("Sequence"));
	AddKind(TEXT("InputAction"), TEXT("IA"), TEXT("Input"));
	AddKind(TEXT("InputMappingContext"), TEXT("IMC"), TEXT("Input"));
	AddKind(TEXT("GameplayAbility"), TEXT("GA"), TEXT("GA"));
	AddKind(TEXT("GameplayEffect"), TEXT("GE"), TEXT("GE"));
	AddKind(TEXT("UserEnum"), TEXT("E"), TEXT("Data"));
	AddKind(TEXT("UserStruct"), TEXT("ST"), TEXT("Data"));
	AddKind(TEXT("Font"), TEXT("FNT"), TEXT("UI"));
}

FDataForgeResult FDataForgeNamingPolicyResolver::ValidatePolicy(const UDataForgeNamingPolicy& Policy)
{
	FDataForgeResult Result;
	if (!DataForgeNamingPolicy::IsIdentifier(Policy.ProjectPrefix))
	{
		DataForgeNamingPolicy::AddError(Result.Diagnostics, TEXT("DF1701"), TEXT("Naming Policy Project Prefix must be a non-empty alphanumeric value."));
	}

	TSet<FName> KindIds;
	TSet<FString> TypePrefixes;
	for (const FDataForgeAssetKindNamingRule& Rule : Policy.AssetKinds)
	{
		if (Rule.AssetKind.IsNone() || !DataForgeNamingPolicy::IsIdentifier(Rule.TypePrefix))
		{
			DataForgeNamingPolicy::AddError(Result.Diagnostics, TEXT("DF1702"), TEXT("Every Asset Kind requires an id and an alphanumeric Type Prefix."));
			continue;
		}
		if (KindIds.Contains(Rule.AssetKind) || TypePrefixes.Contains(Rule.TypePrefix))
		{
			DataForgeNamingPolicy::AddError(Result.Diagnostics, TEXT("DF1703"), FString::Printf(TEXT("Asset Kind '%s' or Type Prefix '%s' is duplicated."), *Rule.AssetKind.ToString(), *Rule.TypePrefix));
		}
		KindIds.Add(Rule.AssetKind);
		TypePrefixes.Add(Rule.TypePrefix);
	}
	if (Policy.AssetKinds.IsEmpty())
	{
		DataForgeNamingPolicy::AddError(Result.Diagnostics, TEXT("DF1704"), TEXT("Naming Policy requires at least one Asset Kind."));
	}

	Result.bSuccess = !DataForgeNamingPolicy::HasErrors(Result.Diagnostics);
	Result.Summary = Result.bSuccess ? TEXT("Naming Policy is valid.") : TEXT("Naming Policy is invalid.");
	return Result;
}

FDataForgeNamingResult FDataForgeNamingPolicyResolver::Parse(
	const UDataForgeNamingPolicy& Policy,
	const FString& AssetName,
	const FDataForgeNamingParseContext& Context)
{
	FDataForgeNamingResult Result;
	Result.AssetName = AssetName;
	const FDataForgeResult PolicyValidation = ValidatePolicy(Policy);
	Result.Diagnostics.Append(PolicyValidation.Diagnostics);
	if (!PolicyValidation.bSuccess) return Result;

	FString TypePrefix;
	FString QualifiedName;
	if (!AssetName.Split(TEXT("_"), &TypePrefix, &QualifiedName) || TypePrefix.IsEmpty() || QualifiedName.IsEmpty())
	{
		DataForgeNamingPolicy::AddError(Result.Diagnostics, TEXT("DF1705"), TEXT("Asset name must use the format <TypePrefix>_<ProjectPrefix><Name>[_Numbering]."));
		return Result;
	}

	const FDataForgeAssetKindNamingRule* KindRule = Context.AssetKind.IsNone()
		? DataForgeNamingPolicy::FindKindByPrefix(Policy, TypePrefix)
		: DataForgeNamingPolicy::FindKindById(Policy, Context.AssetKind);
	if (!KindRule)
	{
		DataForgeNamingPolicy::AddError(Result.Diagnostics, TEXT("DF1706"), FString::Printf(TEXT("No Asset Kind matches '%s'."), Context.AssetKind.IsNone() ? *TypePrefix : *Context.AssetKind.ToString()));
		return Result;
	}
	Result.Identity.AssetKind = KindRule->AssetKind;
	Result.Identity.TypePrefix = KindRule->TypePrefix;
	Result.Identity.ProjectPrefix = Policy.ProjectPrefix;
	if (KindRule->TypePrefix != TypePrefix)
	{
		DataForgeNamingPolicy::AddError(Result.Diagnostics, TEXT("DF1707"), FString::Printf(TEXT("Asset Kind '%s' requires Type Prefix '%s', not '%s'."), *KindRule->AssetKind.ToString(), *KindRule->TypePrefix, *TypePrefix));
	}

	if (Context.ActualAssetClass)
	{
		UClass* ExpectedClass = KindRule->ExpectedAssetClass.LoadSynchronous();
		if (ExpectedClass && !Context.ActualAssetClass->IsChildOf(ExpectedClass))
		{
			DataForgeNamingPolicy::AddError(Result.Diagnostics, TEXT("DF1708"), FString::Printf(TEXT("Asset class '%s' is incompatible with Asset Kind '%s' (%s expected)."), *Context.ActualAssetClass->GetName(), *KindRule->AssetKind.ToString(), *ExpectedClass->GetName()));
		}
	}

	int32 Numbering = INDEX_NONE;
	int32 LastUnderscore = INDEX_NONE;
	if (QualifiedName.FindLastChar(TEXT('_'), LastUnderscore))
	{
		const FString Suffix = QualifiedName.Mid(LastUnderscore + 1);
		const bool bAllDigits = DataForgeNamingPolicy::IsDigits(Suffix);
		if (bAllDigits)
		{
			if (Suffix.Len() > 1 && Suffix[0] == TEXT('0'))
			{
				DataForgeNamingPolicy::AddError(Result.Diagnostics, TEXT("DF1709"), TEXT("Numbering must not contain leading zeroes."));
			}
			Numbering = FCString::Atoi(*Suffix);
			QualifiedName.LeftInline(LastUnderscore);
		}
	}

	if (!QualifiedName.StartsWith(Policy.ProjectPrefix, ESearchCase::CaseSensitive))
	{
		DataForgeNamingPolicy::AddError(Result.Diagnostics, TEXT("DF1711"), FString::Printf(TEXT("Asset name must include project prefix '%s' immediately after the type prefix."), *Policy.ProjectPrefix));
		return Result;
	}

	const FString LogicalName = QualifiedName.Mid(Policy.ProjectPrefix.Len());
	if (LogicalName.IsEmpty())
	{
		DataForgeNamingPolicy::AddError(Result.Diagnostics, TEXT("DF1712"), TEXT("Asset name has no subject after the project prefix."));
		return Result;
	}

	FString Subject = Context.Subject;
	FString Remaining = LogicalName;
	if (Subject.IsEmpty())
	{
		Subject = LogicalName;
		Remaining.Reset();
	}
	else if (LogicalName.StartsWith(Subject, ESearchCase::CaseSensitive))
	{
		Remaining.RightChopInline(Subject.Len());
	}
	else
	{
		DataForgeNamingPolicy::AddError(Result.Diagnostics, TEXT("DF1713"), FString::Printf(TEXT("Asset name subject does not match expected Subject '%s'."), *Subject));
		return Result;
	}

	FString Role = Context.Role;
	FString Variant;
	if (Role.IsEmpty())
	{
		Role = Remaining;
	}
	else if (Remaining.StartsWith(Role, ESearchCase::CaseSensitive))
	{
		Variant = Remaining.Mid(Role.Len());
	}
	else
	{
		DataForgeNamingPolicy::AddError(Result.Diagnostics, TEXT("DF1714"), FString::Printf(TEXT("Asset name role does not match expected Role '%s'."), *Role));
		return Result;
	}

	DataForgeNamingPolicy::ValidateComponent(TEXT("Subject"), Subject, false, Result.Diagnostics);
	DataForgeNamingPolicy::ValidateComponent(TEXT("Role"), Role, true, Result.Diagnostics);
	DataForgeNamingPolicy::ValidateComponent(TEXT("Variant"), Variant, true, Result.Diagnostics);

	Result.Identity.AssetKind = KindRule->AssetKind;
	Result.Identity.TypePrefix = KindRule->TypePrefix;
	Result.Identity.ProjectPrefix = Policy.ProjectPrefix;
	Result.Identity.Subject = MoveTemp(Subject);
	Result.Identity.Role = MoveTemp(Role);
	Result.Identity.Variant = MoveTemp(Variant);
	Result.Identity.Numbering = Numbering;
	Result.bSuccess = !DataForgeNamingPolicy::HasErrors(Result.Diagnostics);
	return Result;
}

FDataForgeNamingResult FDataForgeNamingPolicyResolver::Build(
	const UDataForgeNamingPolicy& Policy,
	const FDataForgeAssetIdentity& Identity)
{
	FDataForgeNamingResult Result;
	Result.Identity = Identity;
	const FDataForgeResult PolicyValidation = ValidatePolicy(Policy);
	Result.Diagnostics.Append(PolicyValidation.Diagnostics);
	if (!PolicyValidation.bSuccess) return Result;

	const FDataForgeAssetKindNamingRule* KindRule = DataForgeNamingPolicy::FindKindById(Policy, Identity.AssetKind);
	if (!KindRule)
	{
		DataForgeNamingPolicy::AddError(Result.Diagnostics, TEXT("DF1706"), FString::Printf(TEXT("No Asset Kind matches '%s'."), *Identity.AssetKind.ToString()));
		return Result;
	}
	if (!Identity.TypePrefix.IsEmpty() && Identity.TypePrefix != KindRule->TypePrefix)
	{
		DataForgeNamingPolicy::AddError(Result.Diagnostics, TEXT("DF1707"), FString::Printf(TEXT("Asset Kind '%s' requires Type Prefix '%s', not '%s'."), *KindRule->AssetKind.ToString(), *KindRule->TypePrefix, *Identity.TypePrefix));
	}
	if (!Identity.ProjectPrefix.IsEmpty() && Identity.ProjectPrefix != Policy.ProjectPrefix)
	{
		DataForgeNamingPolicy::AddError(Result.Diagnostics, TEXT("DF1711"), FString::Printf(TEXT("Identity Project Prefix must be '%s'."), *Policy.ProjectPrefix));
	}
	DataForgeNamingPolicy::ValidateComponent(TEXT("Subject"), Identity.Subject, false, Result.Diagnostics);
	DataForgeNamingPolicy::ValidateComponent(TEXT("Role"), Identity.Role, true, Result.Diagnostics);
	DataForgeNamingPolicy::ValidateComponent(TEXT("Variant"), Identity.Variant, true, Result.Diagnostics);
	if (Identity.Numbering < INDEX_NONE)
	{
		DataForgeNamingPolicy::AddError(Result.Diagnostics, TEXT("DF1715"), TEXT("Numbering must be absent or non-negative."));
	}
	if (DataForgeNamingPolicy::HasErrors(Result.Diagnostics)) return Result;

	Result.Identity.TypePrefix = KindRule->TypePrefix;
	Result.Identity.ProjectPrefix = Policy.ProjectPrefix;
	Result.AssetName = KindRule->TypePrefix + TEXT("_") + Policy.ProjectPrefix + Identity.Subject + Identity.Role + Identity.Variant;
	if (Identity.Numbering != INDEX_NONE)
	{
		Result.AssetName += TEXT("_") + FString::FromInt(Identity.Numbering);
	}
	Result.bSuccess = true;
	return Result;
}
