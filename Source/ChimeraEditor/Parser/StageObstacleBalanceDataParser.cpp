#include "Parser/StageObstacleBalanceDataParser.h"

#include "Parser/SheetDataTableUtils.h"
#include "Parser/SheetValidation.h"
#include "Stage/Obstacle/Data/CMObstacleBalanceTableRow.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraStageObstacleBalanceParser, Log, All);

bool UStageObstacleBalanceDataParser::OnParseComplete(FString& OutError)
{
    using namespace SheetDataTableUtils;
    using namespace SheetValidation;
    static constexpr const TCHAR* ParserName = TEXT("StageObstacleBalanceData");
    static const TArray<FString> RequiredHeaders =
        { TEXT("RowName"), TEXT("ID"), TEXT("DamageMultiplier") };

    FParseReport Report;
    OutError.Reset();
    if (!ValidateTargetTable(ParserName, TargetTable,
            FCMStageObstacleBalanceTableRow::StaticStruct(), OutError))
    {
        return false;
    }
    ValidateRequiredHeaders(GetHeaders(), RequiredHeaders, &Report);
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
        const FName RowName = Reader.GetRequiredName(TEXT("RowName"));
        FCMStageObstacleBalanceTableRow Row;
        Row.ID = Reader.GetRequiredName(TEXT("ID"));
        const FString Source = Reader.GetRequiredString(TEXT("DamageMultiplier"));
        const bool bValidMultiplier = LexTryParseString(Row.DamageMultiplier, *Source)
            && FMath::IsFinite(Row.DamageMultiplier)
            && Row.DamageMultiplier >= 0.0f;
        if (!bValidMultiplier)
        {
            Report.AddIssue(EParseIssueSeverity::Error,
                TEXT("DamageMultiplier는 0 이상의 유한 숫자여야 합니다."),
                Index, RowName, TEXT("DamageMultiplier"), Source);
        }
        if (!Row.ID.IsNone() && IDs.Contains(Row.ID))
        {
            Report.AddIssue(EParseIssueSeverity::Error,
                TEXT("ID는 시트에서 고유해야 합니다."), Index, RowName,
                TEXT("ID"), Row.ID.ToString());
        }
        if (!RowName.IsNone() && RowNames.Contains(RowName))
        {
            Report.AddIssue(EParseIssueSeverity::Error,
                TEXT("RowName은 시트에서 고유해야 합니다."), Index, RowName,
                TEXT("RowName"), RowName.ToString());
        }
        if (!Reader.IsValid() || Report.ErrorCount != ErrorCountBeforeRow)
        {
            continue;
        }
        IDs.Add(Row.ID);
        RowNames.Add(RowName);
        TargetTable->AddRow(RowName, Row);
        Report.AddSuccess();
        UE_LOG(LogChimeraStageObstacleBalanceParser, Verbose,
            TEXT("[CSV -> Stage Obstacle Balance] Row=%s Multiplier=%.2f"),
            *RowName.ToString(), Row.DamageMultiplier);
    }
    return FinalizeParseReport(ParserName, Report, OutError);
}
