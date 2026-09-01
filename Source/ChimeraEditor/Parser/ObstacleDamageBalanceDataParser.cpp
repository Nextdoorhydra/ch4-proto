#include "Parser/ObstacleDamageBalanceDataParser.h"

#include "Parser/SheetDataTableUtils.h"
#include "Parser/SheetValidation.h"
#include "Stage/Obstacle/Data/CMObstacleBalanceTableRow.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraObstacleDamageBalanceParser, Log, All);

namespace ObstacleDamageBalanceColumns
{
    const FString RowName = TEXT("RowName");
    const FString ID = TEXT("ID");
    const FString ObstacleType = TEXT("ObstacleType");
    const FString DefaultDamage = TEXT("DefaultDamage");
    const FString HighDamage = TEXT("HighDamage");
    const TArray<FString> RequiredHeaders =
        { RowName, ID, ObstacleType, DefaultDamage, HighDamage };
}

bool UObstacleDamageBalanceDataParser::OnParseComplete(FString& OutError)
{
    using namespace SheetDataTableUtils;
    using namespace SheetValidation;
    static constexpr const TCHAR* ParserName = TEXT("ObstacleDamageBalanceData");
    FParseReport Report;
    OutError.Reset();
    if (!ValidateTargetTable(ParserName, TargetTable,
            FCMObstacleDamageBalanceTableRow::StaticStruct(), OutError))
    {
        return false;
    }
    ValidateRequiredHeaders(GetHeaders(),
        ObstacleDamageBalanceColumns::RequiredHeaders, &Report);
    if (Report.HasErrors())
    {
        return FinalizeParseReport(ParserName, Report, OutError);
    }

    FScopedDataTableEditNotification Edit(TargetTable);
    TSet<FName> IDs;
    TSet<FName> RowNames;
    for (int32 Index = 0; Index < GetRowCount(); ++Index)
    {
        const int32 ErrorCountBeforeRow = Report.ErrorCount;
        TMap<FString, FString> Data;
        if (!GetRowAt(Index, Data)) continue;
        FSheetRowReader Reader(Data, Index, Report);
        const FName ParsedRowName = Reader.GetRequiredName(
            ObstacleDamageBalanceColumns::RowName);
        FCMObstacleDamageBalanceTableRow Row;
        Row.ID = Reader.GetRequiredName(ObstacleDamageBalanceColumns::ID);

        const FString TypeSource = Reader.GetRequiredString(
            ObstacleDamageBalanceColumns::ObstacleType);
        const int64 TypeValue = StaticEnum<ECMObstacleBalanceType>()
            ->GetValueByNameString(TypeSource);
        if (TypeValue == INDEX_NONE)
        {
            Report.AddIssue(EParseIssueSeverity::Error,
                TEXT("지원하지 않는 ObstacleType입니다."), Index,
                ParsedRowName, ObstacleDamageBalanceColumns::ObstacleType,
                TypeSource);
        }
        else
        {
            Row.ObstacleType = static_cast<ECMObstacleBalanceType>(TypeValue);
        }

        auto ReadDamage = [&](const FString& Column, float& OutValue)
        {
            const FString Source = Reader.GetRequiredString(Column);
            if (!LexTryParseString(OutValue, *Source)
                || !FMath::IsFinite(OutValue) || OutValue < 0.0f)
            {
                Report.AddIssue(EParseIssueSeverity::Error,
                    TEXT("0 이상의 유한 숫자여야 합니다."), Index,
                    ParsedRowName, Column, Source);
            }
        };
        ReadDamage(ObstacleDamageBalanceColumns::DefaultDamage,
            Row.DefaultDamage);
        ReadDamage(ObstacleDamageBalanceColumns::HighDamage,
            Row.HighDamage);
        if (Row.HighDamage < Row.DefaultDamage)
        {
            Report.AddIssue(EParseIssueSeverity::Error,
                TEXT("HighDamage는 DefaultDamage 이상이어야 합니다."),
                Index, ParsedRowName,
                ObstacleDamageBalanceColumns::HighDamage);
        }
        if (!Row.ID.IsNone() && IDs.Contains(Row.ID))
        {
            Report.AddIssue(EParseIssueSeverity::Error,
                TEXT("ID는 시트에서 고유해야 합니다."), Index,
                ParsedRowName, ObstacleDamageBalanceColumns::ID,
                Row.ID.ToString());
        }
        if (!ParsedRowName.IsNone() && RowNames.Contains(ParsedRowName))
        {
            Report.AddIssue(EParseIssueSeverity::Error,
                TEXT("RowName은 시트에서 고유해야 합니다."), Index,
                ParsedRowName, ObstacleDamageBalanceColumns::RowName,
                ParsedRowName.ToString());
        }
        if (!Reader.IsValid() || Report.ErrorCount != ErrorCountBeforeRow)
        {
            continue;
        }
        IDs.Add(Row.ID);
        RowNames.Add(ParsedRowName);
        TargetTable->AddRow(ParsedRowName, Row);
        Report.AddSuccess();
        UE_LOG(LogChimeraObstacleDamageBalanceParser, Verbose,
            TEXT("[CSV -> Obstacle Damage] Row=%s Default=%.2f High=%.2f"),
            *ParsedRowName.ToString(), Row.DefaultDamage, Row.HighDamage);
    }
    return FinalizeParseReport(ParserName, Report, OutError);
}
