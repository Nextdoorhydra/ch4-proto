#include "NKMTextRef.h"

#include "Internationalization/Text.h"
#include "NKMLocalizationSettings.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NKMTextRef)

namespace
{
	bool IsValidAlias(const FString& Alias)
	{
		if (!GetDefault<UNKMLocalizationSettings>()->IsAllowedTableId(Alias))
		{
			return false;
		}
		for (const TCHAR Character : Alias)
		{
			if (!FChar::IsAlnum(Character) && Character != TEXT('_') && Character != TEXT('.'))
			{
				return false;
			}
		}
		return true;
	}
}

FString FNKMTextRef::ToString() const
{
	return IsSet() ? FString::Printf(TEXT("%s::%s"), *TableId.ToString(), *Key) : FString();
}

FText FNKMTextRef::Resolve() const
{
	FName RuntimeTableId;
	if (!IsSet() || !ResolveTableId(TableId, RuntimeTableId))
	{
		return FText::GetEmpty();
	}
	// String Tables are preloaded by UNKMLocalizationSubsystem. Gameplay resolution
	// must never introduce a blocking asset load.
	return FText::FromStringTable(RuntimeTableId, Key, EStringTableLoadingPolicy::Find);
}

bool FNKMTextRef::Parse(const FString& Value, const FName DefaultTableId, FNKMTextRef& OutRef, FString* OutError)
{
	const FString CleanValue = Value.TrimStartAndEnd();
	FString TableString;
	FString KeyString;
	if (!CleanValue.Split(TEXT("::"), &TableString, &KeyString, ESearchCase::CaseSensitive, ESearchDir::FromStart))
	{
		TableString = DefaultTableId.ToString();
		KeyString = CleanValue;
	}

	TableString.TrimStartAndEndInline();
	KeyString.TrimStartAndEndInline();
	if (TableString.IsEmpty() || KeyString.IsEmpty() || KeyString.Contains(TEXT("::")))
	{
		if (OutError)
		{
			*OutError = TEXT("Text reference must be 'Table.Alias::Key', or 'Key' with a default table.");
		}
		return false;
	}

	const FName ParsedTable(*TableString);
	FName RuntimeTableId;
	if (!ResolveTableId(ParsedTable, RuntimeTableId))
	{
		if (OutError)
		{
			*OutError = FString::Printf(TEXT("Unknown or invalid String Table alias '%s'."), *TableString);
		}
		return false;
	}

	OutRef = FNKMTextRef(ParsedTable, MoveTemp(KeyString));
	return true;
}

bool FNKMTextRef::ResolveTableId(const FName AliasOrObjectPath, FName& OutRuntimeTableId)
{
	if (AliasOrObjectPath.IsNone())
	{
		return false;
	}

	const FString Value = AliasOrObjectPath.ToString();
	if (Value.StartsWith(TEXT("/Game/"), ESearchCase::CaseSensitive) && Value.Contains(TEXT(".")))
	{
		OutRuntimeTableId = AliasOrObjectPath;
		return true;
	}

	if (GetDefault<UNKMLocalizationSettings>()->TryResolveOverride(AliasOrObjectPath, OutRuntimeTableId))
	{
		return true;
	}

	if (!IsValidAlias(Value))
	{
		return false;
	}

	OutRuntimeTableId = FName(*GetDefault<UNKMLocalizationSettings>()->MakeConventionalTableObjectPath(AliasOrObjectPath));
	return true;
}

bool FNKMTextRef::ExportTextItem(
	FString& ValueStr,
	const FNKMTextRef& DefaultValue,
	UObject* Parent,
	const int32 PortFlags,
	UObject* ExportRootScope) const
{
	const FString Exported = ToString();
	ValueStr += (PortFlags & PPF_Delimited)
		? FString::Printf(TEXT("\"%s\""), *Exported.ReplaceCharWithEscapedChar())
		: Exported;
	return true;
}

bool FNKMTextRef::ImportTextItem(const TCHAR*& Buffer, const int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
{
	FString Imported;
	const TCHAR* NewBuffer = FPropertyHelpers::ReadToken(Buffer, Imported, true);
	if (!NewBuffer)
	{
		return false;
	}

	FNKMTextRef Parsed;
	FString Error;
	if (!Parse(Imported, NAME_None, Parsed, &Error))
	{
		if (ErrorText)
		{
			ErrorText->Logf(TEXT("FNKMTextRef: %s"), *Error);
		}
		return false;
	}

	*this = MoveTemp(Parsed);
	Buffer = NewBuffer;
	return true;
}

FText UNKMLocalizationTextLibrary::ResolveTextReference(const FNKMTextRef& TextReference)
{
	return TextReference.Resolve();
}

bool UNKMLocalizationTextLibrary::ParseTextReference(
	const FString& Value,
	const FName DefaultTableId,
	FNKMTextRef& OutReference)
{
	return FNKMTextRef::Parse(Value, DefaultTableId, OutReference);
}

bool UNKMLocalizationTextLibrary::MakeBoundTextReference(
	const FName BindingProfile,
	const FName RecordId,
	const FName FieldName,
	FNKMTextRef& OutReference)
{
	return GetDefault<UNKMLocalizationSettings>()->TryMakeTextReference(
		BindingProfile,
		RecordId,
		FieldName,
		OutReference);
}

FText UNKMLocalizationTextLibrary::ResolveBoundText(
	const FName BindingProfile,
	const FName RecordId,
	const FName FieldName)
{
	return GetDefault<UNKMLocalizationSettings>()->ResolveBoundText(
		BindingProfile,
		RecordId,
		FieldName);
}
