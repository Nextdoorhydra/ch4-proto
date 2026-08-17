#include "NKMTranslationStatusAnalyzer.h"

#include "Internationalization/Text.h"
#include "Misc/FileHelper.h"
#include "PortableObjectFormatDOM.h"

namespace
{
	bool HasSameFormatArguments(const FString& Source, const FString& Translation)
	{
		const FTextFormat SourceFormat = FTextFormat::FromString(Source);
		const FTextFormat TranslationFormat = FTextFormat::FromString(Translation);
		if (!SourceFormat.IsValid() || !TranslationFormat.IsValid())
		{
			return false;
		}

		TArray<FString> SourceArguments;
		TArray<FString> TranslationArguments;
		SourceFormat.GetFormatArgumentNames(SourceArguments);
		TranslationFormat.GetFormatArgumentNames(TranslationArguments);
		SourceArguments.Sort();
		TranslationArguments.Sort();
		return SourceArguments == TranslationArguments;
	}
}

bool FNKMTranslationStatusAnalyzer::AnalyzeFile(
	const FString& Filename,
	FNKMTranslationStatus& OutStatus,
	FString& OutError)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Filename))
	{
		OutError = FString::Printf(TEXT("Unable to read PO '%s'."), *Filename);
		return false;
	}
	return AnalyzeString(Text, OutStatus, OutError);
}

bool FNKMTranslationStatusAnalyzer::AnalyzeString(
	const FString& PortableObjectText,
	FNKMTranslationStatus& OutStatus,
	FString& OutError)
{
	OutStatus = {};
	FPortableObjectFormatDOM PortableObject;
	FText ParseError;
	if (!PortableObject.FromString(PortableObjectText, &ParseError))
	{
		OutError = ParseError.ToString();
		return false;
	}
	int32 FuzzyEntryCount = 0;
	TArray<FString> Lines;
	PortableObjectText.ParseIntoArrayLines(Lines, false);
	for (const FString& Line : Lines)
	{
		if (Line.StartsWith(TEXT("#,")) && Line.Contains(TEXT("fuzzy"), ESearchCase::IgnoreCase))
		{
			++FuzzyEntryCount;
		}
	}

	for (auto Iterator = PortableObject.GetEntriesIterator(); Iterator; ++Iterator)
	{
		const TSharedPtr<FPortableObjectEntry>& Entry = Iterator.Value();
		if (!Entry.IsValid() || Entry->MsgId.IsEmpty())
		{
			continue;
		}

		++OutStatus.Total;
		const bool bMissing = Entry->MsgStr.IsEmpty()
			|| !Entry->MsgStr.ContainsByPredicate([](const FString& Value) { return !Value.IsEmpty(); });

		bool bInvalid = false;
		if (!bMissing)
		{
			for (const FString& Translation : Entry->MsgStr)
			{
				if (!Translation.IsEmpty() && !HasSameFormatArguments(Entry->MsgId, Translation))
				{
					bInvalid = true;
					break;
				}
			}
		}

		if (bInvalid) ++OutStatus.Invalid;
		else if (bMissing) ++OutStatus.Missing;
		else ++OutStatus.Translated;
	}

	// UE 5.7's PO DOM does not reliably retain '#, fuzzy' in Flags, so count
	// those markers from the source text and keep the dashboard categories exclusive.
	int32 RemainingFuzzy = FMath::Min(FuzzyEntryCount, OutStatus.Total - OutStatus.Invalid);
	const int32 FromTranslated = FMath::Min(RemainingFuzzy, OutStatus.Translated);
	OutStatus.Translated -= FromTranslated;
	OutStatus.Stale += FromTranslated;
	RemainingFuzzy -= FromTranslated;
	const int32 FromMissing = FMath::Min(RemainingFuzzy, OutStatus.Missing);
	OutStatus.Missing -= FromMissing;
	OutStatus.Stale += FromMissing;
	return true;
}
