#include "Parser/ObstacleStatusBalanceDataParser.h"

#include "Parser/SheetDataTableUtils.h"
#include "Parser/SheetValidation.h"
#include "Stage/Obstacle/Data/CMObstacleBalanceTableRow.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraObstacleStatusBalanceParser, Log, All);

bool UObstacleStatusBalanceDataParser::OnParseComplete(FString& OutError)
{
    using namespace SheetDataTableUtils;
    using namespace SheetValidation;
    static constexpr const TCHAR* ParserName = TEXT("ObstacleStatusBalanceData");
    static const TArray<FString> RequiredHeaders = {
        TEXT("RowName"), TEXT("ID"), TEXT("StatusEffect"),
        TEXT("DefaultDuration"), TEXT("PrimaryStatusValue"),
        TEXT("SecondaryStatusValue") };
    FParseReport Report;
    OutError.Reset();
    if (!ValidateTargetTable(ParserName, TargetTable,
            FCMObstacleStatusBalanceTableRow::StaticStruct(), OutError))
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
        const FName ParsedRowName = Reader.GetRequiredName(TEXT("RowName"));
        FCMObstacleStatusBalanceTableRow Row;
        Row.ID = Reader.GetRequiredName(TEXT("ID"));

        const FString StatusSource = Reader.GetRequiredString(TEXT("StatusEffect"));
        const int64 StatusValue = StaticEnum<ECMObstacleStatusEffect>()
            ->GetValueByNameString(StatusSource);
        if (StatusValue == INDEX_NONE
            || StatusValue == static_cast<int64>(ECMObstacleStatusEffect::None))
        {
            Report.AddIssue(EParseIssueSeverity::Error,
                TEXT("상태 전용 표에는 None이 아닌 지원 상태를 입력해야 합니다."),
                Index, ParsedRowName, TEXT("StatusEffect"), StatusSource);
        }
        else
        {
            Row.StatusEffect = static_cast<ECMObstacleStatusEffect>(StatusValue);
        }

        auto ReadValue = [&](const TCHAR* Column, float& OutValue)
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
        ReadValue(TEXT("DefaultDuration"), Row.DefaultDuration);
        ReadValue(TEXT("PrimaryStatusValue"), Row.PrimaryStatusValue);
        ReadValue(TEXT("SecondaryStatusValue"), Row.SecondaryStatusValue);

        if (Row.StatusEffect == ECMObstacleStatusEffect::PartSlowed
            && Row.PrimaryStatusValue > 1.0f)
        {
            Report.AddIssue(EParseIssueSeverity::Error,
                TEXT("PartSlowed의 PrimaryStatusValue는 0~1이어야 합니다."),
                Index, ParsedRowName, TEXT("PrimaryStatusValue"));
        }
        if (Row.StatusEffect == ECMObstacleStatusEffect::BodyBlinded
            && !FMath::IsNearlyEqual(Row.PrimaryStatusValue,
                FMath::RoundToFloat(Row.PrimaryStatusValue)))
        {
            Report.AddIssue(EParseIssueSeverity::Error,
                TEXT("BodyBlinded의 PrimaryStatusValue는 정수여야 합니다."),
                Index, ParsedRowName, TEXT("PrimaryStatusValue"));
        }
        if (Row.StatusEffect == ECMObstacleStatusEffect::BodyVisionReduced
            && (Row.PrimaryStatusValue <= 0.0f || Row.PrimaryStatusValue > 1.0f
                || Row.SecondaryStatusValue <= 0.0f
                || Row.SecondaryStatusValue > 1.0f))
        {
            Report.AddIssue(EParseIssueSeverity::Error,
                TEXT("BodyVisionReduced의 두 배율은 0 초과 1 이하여야 합니다."),
                Index, ParsedRowName, TEXT("PrimaryStatusValue"));
        }
        if (!Row.ID.IsNone() && IDs.Contains(Row.ID))
        {
            Report.AddIssue(EParseIssueSeverity::Error,
                TEXT("ID는 시트에서 고유해야 합니다."), Index,
                ParsedRowName, TEXT("ID"), Row.ID.ToString());
        }
        if (!ParsedRowName.IsNone() && RowNames.Contains(ParsedRowName))
        {
            Report.AddIssue(EParseIssueSeverity::Error,
                TEXT("RowName은 시트에서 고유해야 합니다."), Index,
                ParsedRowName, TEXT("RowName"), ParsedRowName.ToString());
        }
        if (!Reader.IsValid() || Report.ErrorCount != ErrorCountBeforeRow)
        {
            continue;
        }
        IDs.Add(Row.ID);
        RowNames.Add(ParsedRowName);
        TargetTable->AddRow(ParsedRowName, Row);
        Report.AddSuccess();
        UE_LOG(LogChimeraObstacleStatusBalanceParser, Verbose,
            TEXT("[CSV -> Obstacle Status] Row=%s Duration=%.2f"),
            *ParsedRowName.ToString(), Row.DefaultDuration);
    }
    return FinalizeParseReport(ParserName, Report, OutError);
}
