#include "NKMLocalizationSettings.h"

#include "Misc/Paths.h"
#include "NKMTextRef.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NKMLocalizationSettings)

bool UNKMLocalizationSettings::TryResolveOverride(const FName Alias, FName& OutTableId) const
{
	for (const FNKMStringTableAliasOverride& Override : TableAliasOverrides)
	{
		if (Override.Alias == Alias && Override.TableAsset.IsValid())
		{
			OutTableId = FName(*Override.TableAsset.ToString());
			return true;
		}
	}
	return false;
}

const FNKMTextBindingProfile* UNKMLocalizationSettings::FindTextBindingProfile(const FName ProfileName) const
{
	return TextBindingProfiles.FindByPredicate(
		[ProfileName](const FNKMTextBindingProfile& Profile)
		{
			return Profile.ProfileName == ProfileName;
		});
}

bool UNKMLocalizationSettings::TryMakeTextReference(
	const FName ProfileName,
	const FName RecordId,
	const FName FieldName,
	FNKMTextRef& OutReference) const
{
	const FNKMTextBindingProfile* Profile = FindTextBindingProfile(ProfileName);
	if (!Profile || Profile->TableId.IsNone() || RecordId.IsNone() || FieldName.IsNone())
	{
		return false;
	}
	if (!Profile->Fields.ContainsByPredicate(
		[FieldName](const FNKMTextBindingField& Field) { return Field.FieldName == FieldName; }))
	{
		return false;
	}

	FString Key = Profile->KeyPattern;
	Key.ReplaceInline(TEXT("{Id}"), *RecordId.ToString(), ESearchCase::CaseSensitive);
	Key.ReplaceInline(TEXT("{Field}"), *FieldName.ToString(), ESearchCase::CaseSensitive);
	if (Key.IsEmpty() || Key.Contains(TEXT("{")) || Key.Contains(TEXT("}")))
	{
		return false;
	}

	OutReference = FNKMTextRef(Profile->TableId, MoveTemp(Key));
	return true;
}

FText UNKMLocalizationSettings::ResolveBoundText(
	const FName ProfileName,
	const FName RecordId,
	const FName FieldName) const
{
	FNKMTextRef Reference;
	return TryMakeTextReference(ProfileName, RecordId, FieldName, Reference)
		? Reference.Resolve()
		: FText::GetEmpty();
}

bool UNKMLocalizationSettings::IsAllowedTableId(const FString& TableId) const
{
	return TableIdPrefix.IsEmpty()
		|| TableId.Equals(TableIdPrefix, ESearchCase::CaseSensitive)
		|| TableId.StartsWith(TableIdPrefix + TEXT("."), ESearchCase::CaseSensitive);
}

FString UNKMLocalizationSettings::GetNormalizedStringTableAssetRoot() const
{
	FString Result = StringTableAssetRoot.Path.TrimStartAndEnd();
	while (Result.EndsWith(TEXT("/"))) Result.LeftChopInline(1);
	return Result;
}

FString UNKMLocalizationSettings::GetNormalizedAuthoringSourceRoot() const
{
	return TEXT("Content/Localization/Source");
}

FString UNKMLocalizationSettings::GetNormalizedLocalizationTargetRoot() const
{
	return TEXT("Content/Localization") / LocalizationTargetName;
}

FString UNKMLocalizationSettings::MakeConventionalTablePackagePath(const FName Alias) const
{
	FString AssetName = Alias.ToString().Replace(TEXT("."), TEXT("_"));
	AssetName = StringTableAssetPrefix + AssetName;
	return GetNormalizedStringTableAssetRoot() / AssetName;
}

FString UNKMLocalizationSettings::MakeConventionalTableObjectPath(const FName Alias) const
{
	const FString PackagePath = MakeConventionalTablePackagePath(Alias);
	return FString::Printf(TEXT("%s.%s"), *PackagePath, *FPaths::GetBaseFilename(PackagePath));
}
