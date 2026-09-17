#include "PartLegArmDataParser.h"

#include "Data/Part/CMPartLegArmTableRow.h"
#include "Parser/SheetDataTableUtils.h"
#include "Parser/SheetParserUtils.h"
#include "Parser/SheetValidation.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraPartDataParser, Log, All);

namespace PartColumns
{
    const FString RowName = TEXT("RowName");
    const FString ID = TEXT("ID");
    const FString PartType = TEXT("PartType");
    const FString Species = TEXT("Species");
    const FString MaxHealth = TEXT("MaxHealth");
    const FString Strength = TEXT("Strength");
    const FString Weight = TEXT("Weight");
    const FString StaminaCost = TEXT("StaminaCost");
    const FString StaminaPerSecond = TEXT("Staminapersec");
    const FString ActionDuration = TEXT("ActionDuration");
    const FString BaseMovementImpulse = TEXT("BaseMovementImpulse");
    const FString AttackRange = TEXT("AttackRange");
    const FString AttackRadius = TEXT("AttackRadius");
    const FString ExtensionSpeed = TEXT("ExtensionSpeed");
    const FString PullImpulse = TEXT("PullImpulse");
    const FString SweepHalfAngle = TEXT("SweepHalfAngle");
    const FString SweepSpeed = TEXT("SweepSpeed");

    const TArray<FString> RequiredHeaders =
    {
        RowName,
        ID,
        PartType,
        Species,
        MaxHealth,
        Strength,
        Weight,
        StaminaCost,
        StaminaPerSecond,
        ActionDuration,
        BaseMovementImpulse,
        AttackRange,
        AttackRadius,
        ExtensionSpeed,
        PullImpulse,
        SweepHalfAngle,
        SweepSpeed,
    };

    bool ReadRequiredFloat(
        SheetValidation::FSheetRowReader& Row,
        const FString& ColumnName,
        float& OutValue,
        SheetValidation::FParseReport& Report,
        int32 RowIndex,
        FName ParsedRowName,
        bool bAllowZero)
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

        if (!FMath::IsFinite(OutValue) || OutValue < 0.0f || (!bAllowZero && OutValue == 0.0f))
        {
            Report.AddIssue(
                SheetValidation::EParseIssueSeverity::Error,
                bAllowZero
                    ? TEXT("0 이상의 값이어야 합니다.")
                    : TEXT("0보다 큰 값이어야 합니다."),
                RowIndex,
                ParsedRowName,
                ColumnName,
                SourceValue);
            return false;
        }
        return true;
    }
}

bool UPartLegArmDataParser::OnParseComplete(FString& OutError)
{
    using namespace SheetDataTableUtils;
    using namespace SheetValidation;

    static constexpr const TCHAR* ParserName = TEXT("PartLegArmData");
    FParseReport Report;
    OutError.Reset();

    if (!ValidateTargetTable(
            ParserName,
            TargetTable,
            FCMPartLegArmTableRow::StaticStruct(),
            OutError))
    {
        return false;
    }

    ValidateRequiredHeaders(
        GetHeaders(),
        PartColumns::RequiredHeaders,
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
        FCMPartLegArmTableRow NewRow;
        const FName ParsedRowName =
            Row.GetRequiredName(PartColumns::RowName);
        NewRow.ID = Row.GetRequiredName(PartColumns::ID);
        NewRow.PartType = Row.GetRequiredName(PartColumns::PartType);
        NewRow.Species = Row.GetRequiredName(PartColumns::Species);

        bool bHasValidNumbers = true;
        bHasValidNumbers &= PartColumns::ReadRequiredFloat(
            Row, PartColumns::Weight, NewRow.Weight,
            Report, Index, ParsedRowName, true);
        bHasValidNumbers &= PartColumns::ReadRequiredFloat(
            Row, PartColumns::MaxHealth, NewRow.MaxHealth,
            Report, Index, ParsedRowName, false);
        bHasValidNumbers &= PartColumns::ReadRequiredFloat(
            Row, PartColumns::Strength, NewRow.Strength,
            Report, Index, ParsedRowName, true);
        bHasValidNumbers &= PartColumns::ReadRequiredFloat(
            Row, PartColumns::StaminaCost, NewRow.StaminaCost,
            Report, Index, ParsedRowName, true);
        bHasValidNumbers &= PartColumns::ReadRequiredFloat(
            Row, PartColumns::StaminaPerSecond,
            NewRow.StaminaPerSecond,
            Report, Index, ParsedRowName, true);
        bHasValidNumbers &= PartColumns::ReadRequiredFloat(
            Row, PartColumns::ActionDuration, NewRow.ActionDuration,
            Report, Index, ParsedRowName, false);
        bHasValidNumbers &= PartColumns::ReadRequiredFloat(
            Row, PartColumns::BaseMovementImpulse,
            NewRow.BaseMovementImpulse,
            Report, Index, ParsedRowName, true);
        bHasValidNumbers &= PartColumns::ReadRequiredFloat(
            Row, PartColumns::AttackRange, NewRow.AttackRange,
            Report, Index, ParsedRowName, true);
        bHasValidNumbers &= PartColumns::ReadRequiredFloat(
            Row, PartColumns::AttackRadius, NewRow.AttackRadius,
            Report, Index, ParsedRowName, true);
        bHasValidNumbers &= PartColumns::ReadRequiredFloat(
            Row, PartColumns::ExtensionSpeed, NewRow.ExtensionSpeed,
            Report, Index, ParsedRowName, true);
        bHasValidNumbers &= PartColumns::ReadRequiredFloat(
            Row, PartColumns::PullImpulse, NewRow.PullImpulse,
            Report, Index, ParsedRowName, true);
        bHasValidNumbers &= PartColumns::ReadRequiredFloat(
            Row, PartColumns::SweepHalfAngle, NewRow.SweepHalfAngle,
            Report, Index, ParsedRowName, true);
        bHasValidNumbers &= PartColumns::ReadRequiredFloat(
            Row, PartColumns::SweepSpeed, NewRow.SweepSpeed,
            Report, Index, ParsedRowName, true);

        const bool bKnownPartType = NewRow.PartType == TEXT("Arm")
            || NewRow.PartType == TEXT("Leg");
        if (!bKnownPartType)
        {
            Report.AddIssue(
                EParseIssueSeverity::Error,
                TEXT("PartType은 Arm 또는 Leg여야 합니다."),
                Index,
                ParsedRowName,
                PartColumns::PartType,
                NewRow.PartType.ToString());
        }

        if (!NewRow.ID.IsNone() && ParsedIDs.Contains(NewRow.ID))
        {
            Report.AddIssue(
                EParseIssueSeverity::Error,
                TEXT("ID는 시트 안에서 고유해야 합니다."),
                Index,
                ParsedRowName,
                PartColumns::ID,
                NewRow.ID.ToString());
        }

        if (!Row.IsValid() || !bHasValidNumbers || !bKnownPartType
            || ParsedIDs.Contains(NewRow.ID))
        {
            continue;
        }

        ParsedIDs.Add(NewRow.ID);
        TargetTable->AddRow(ParsedRowName, NewRow);
        Report.AddSuccess();

        UE_LOG(LogChimeraPartDataParser, Verbose,
            TEXT("[CSV -> Part DataTable] Row=%s ID=%s Type=%s Health=%.1f Strength=%.1f Stamina=%.1f StaminaPerSecond=%.1f Action=%.2f BaseImpulse=%.1f"),
            *ParsedRowName.ToString(),
            *NewRow.ID.ToString(),
            *NewRow.PartType.ToString(),
            NewRow.MaxHealth,
            NewRow.Strength,
            NewRow.StaminaCost,
            NewRow.StaminaPerSecond,
            NewRow.ActionDuration,
            NewRow.BaseMovementImpulse);
    }

    return FinalizeParseReport(ParserName, Report, OutError);
}
