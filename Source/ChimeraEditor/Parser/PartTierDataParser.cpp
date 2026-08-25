#include "PartTierDataParser.h"

#include "Data/Part/CMPartTierTableRow.h"
#include "Parser/SheetDataTableUtils.h"
#include "Parser/SheetValidation.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraPartTierDataParser, Log, All);

namespace PartTierColumns
{
    const FString RowName = TEXT("RowName");
    const FString ID = TEXT("ID");
    const FString MovementImpulseMultiplier =
        TEXT("MovementImpulseMultiplier");
    const FString HealthMultiplier = TEXT("HealthMultiplier");
    const FString StrengthMultiplier = TEXT("StrengthMultiplier");

    const TArray<FString> RequiredHeaders =
    {
        RowName,
        ID,
        MovementImpulseMultiplier,
        HealthMultiplier,
        StrengthMultiplier,
    };

    bool ReadPositiveFloat(
        SheetValidation::FSheetRowReader& Row,
        const FString& ColumnName,
        float& OutValue,
        SheetValidation::FParseReport& Report,
        int32 RowIndex,
        FName ParsedRowName)
    {
        const FString SourceValue = Row.GetRequiredString(ColumnName);
        if (SourceValue.IsEmpty())
        {
            return false;
        }

        if (!LexTryParseString(OutValue, *SourceValue))
        {
            Report.AddIssue(
                SheetValidation::EParseIssueSeverity::Error,
                TEXT("숫자로 변환할 수 없는 값입니다."),
                RowIndex,
                ParsedRowName,
                ColumnName,
                SourceValue);
            return false;
        }

        if (OutValue <= 0.0f)
        {
            Report.AddIssue(
                SheetValidation::EParseIssueSeverity::Error,
                TEXT("0보다 큰 배율이어야 합니다."),
                RowIndex,
                ParsedRowName,
                ColumnName,
                SourceValue);
            return false;
        }
        return true;
    }
}

bool UPartTierDataParser::OnParseComplete(FString& OutError)
{
    using namespace SheetDataTableUtils;
    using namespace SheetValidation;

    static constexpr const TCHAR* ParserName = TEXT("PartTierData");
    FParseReport Report;
    OutError.Reset();

    if (!ValidateTargetTable(
            ParserName,
            TargetTable,
            FCMPartTierTableRow::StaticStruct(),
            OutError))
    {
        return false;
    }

    ValidateRequiredHeaders(
        GetHeaders(),
        PartTierColumns::RequiredHeaders,
        &Report
    );
    if (Report.HasErrors())
    {
        return FinalizeParseReport(ParserName, Report, OutError);
    }

    FScopedDataTableEditNotification TableEdit(TargetTable);
    TSet<FName> ParsedIDs;

    for (int32 Index = 0; Index < GetRowCount(); ++Index)
    {
        TMap<FString, FString> RowData;
        if (!GetRowAt(Index, RowData))
        {
            continue;
        }

        FSheetRowReader Row(RowData, Index, Report);
        FCMPartTierTableRow NewRow;
        const FName ParsedRowName =
            Row.GetRequiredName(PartTierColumns::RowName);
        NewRow.ID = Row.GetRequiredName(PartTierColumns::ID);

        bool bHasValidNumbers = true;
        bHasValidNumbers &= PartTierColumns::ReadPositiveFloat(
            Row,
            PartTierColumns::MovementImpulseMultiplier,
            NewRow.MovementImpulseMultiplier,
            Report,
            Index,
            ParsedRowName);
        bHasValidNumbers &= PartTierColumns::ReadPositiveFloat(
            Row,
            PartTierColumns::HealthMultiplier,
            NewRow.HealthMultiplier,
            Report,
            Index,
            ParsedRowName);
        bHasValidNumbers &= PartTierColumns::ReadPositiveFloat(
            Row,
            PartTierColumns::StrengthMultiplier,
            NewRow.StrengthMultiplier,
            Report,
            Index,
            ParsedRowName);

        if (!NewRow.ID.IsNone() && ParsedIDs.Contains(NewRow.ID))
        {
            Report.AddIssue(
                EParseIssueSeverity::Error,
                TEXT("ID는 시트 안에서 고유해야 합니다."),
                Index,
                ParsedRowName,
                PartTierColumns::ID,
                NewRow.ID.ToString());
        }

        if (!Row.IsValid() || !bHasValidNumbers
            || ParsedIDs.Contains(NewRow.ID))
        {
            continue;
        }

        ParsedIDs.Add(NewRow.ID);
        TargetTable->AddRow(ParsedRowName, NewRow);
        Report.AddSuccess();

        UE_LOG(LogChimeraPartTierDataParser, Verbose,
            TEXT("[CSV -> Part Tier] Row=%s ID=%s Move=%.2f Health=%.2f Strength=%.2f"),
            *ParsedRowName.ToString(),
            *NewRow.ID.ToString(),
            NewRow.MovementImpulseMultiplier,
            NewRow.HealthMultiplier,
            NewRow.StrengthMultiplier);
    }

    return FinalizeParseReport(ParserName, Report, OutError);
}
