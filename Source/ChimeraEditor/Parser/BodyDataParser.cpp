// Fill out your copyright notice in the Description page of Project Settings.


#include "BodyDataParser.h"
#include "Data/Body/CMBodyTableRow.h"
#include "Parser/SheetDataTableUtils.h"
#include "Parser/SheetParserUtils.h"
#include "Parser/SheetValidation.h"

DEFINE_LOG_CATEGORY_STATIC(LogChimeraBodyDataParser, Log, All);

namespace BodyColumns
{
    // 프로젝트 규칙상 첫 두 열은 RowName, ID로 고정한다.
    // RowName은 DataTable 검색 키, ID는 게임 데이터의 고유 식별자다.
    const FString RowName = TEXT("RowName");
    const FString ID = TEXT("ID");
    const FString BodyType = TEXT("BodyType");
    const FString SegmentMaxHP = TEXT("SegmentMaxHP");
    const FString MaxStamina = TEXT("MaxStamina");
    const FString StaminaRegen = TEXT("StaminaRegen");
    const FString SegmentMass = TEXT("SegmentMass");
    const FString GroundFriction = TEXT("GroundFriction");
    const FString LinearDamping = TEXT("LinearDamping");
    const FString AngularDamping = TEXT("AngularDamping");
    const FString MaxVelocity = TEXT("MaxVelocity");

    const TArray<FString> RequiredHeaders =
    {
        RowName,
        ID,
        BodyType,
        SegmentMaxHP,
        MaxStamina,
        StaminaRegen,
        SegmentMass,
        GroundFriction,
        LinearDamping,
        AngularDamping,
        MaxVelocity,
    };

    bool ReadRequiredFloat(
        SheetValidation::FSheetRowReader& Row,
        const FString& ColumnName,
        float& OutValue,
        SheetValidation::FParseReport& Report,
        const int32 RowIndex,
        const FName ParsedRowName,
        const bool bAllowZero = false)
    {
        // 시트의 숫자는 먼저 문자열로 도착한다. 빈 값, 문자 입력, 허용 범위를
        // 여기서 차단하여 잘못된 밸런스 값이 런타임 물리에 들어가지 않게 한다.
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

        if (OutValue < 0.0f || (!bAllowZero && OutValue == 0.0f))
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

bool UBodyDataParser::OnParseComplete(FString& OutError)
{
    using namespace SheetDataTableUtils;
    using namespace SheetParserUtils;
    using namespace SheetValidation;

    static constexpr const TCHAR* ParserName = TEXT("BodyData");
    FParseReport Report;
    OutError.Reset();

    if (!ValidateTargetTable(
            ParserName,
            TargetTable,
            FCMBodyTableRow::StaticStruct(),
            OutError))
    {
        return false;
    }

    ValidateRequiredHeaders(GetHeaders(), BodyColumns::RequiredHeaders, &Report);
    if (Report.HasErrors())
    {
        return FinalizeParseReport(ParserName, Report, OutError);
    }

    FScopedDataTableEditNotification TableEdit(TargetTable);

    for (int32 Index = 0; Index < GetRowCount(); ++Index)
    {
        TMap<FString, FString> RowData;
        if (!GetRowAt(Index, RowData))
        {
            continue;
        }

        FSheetRowReader Row(RowData, Index, Report);
        FCMBodyTableRow NewRow;

        const FName ParsedRowName = Row.GetRequiredName(BodyColumns::RowName);
        NewRow.ID = Row.GetRequiredName(BodyColumns::ID);
        NewRow.BodyType = Row.GetRequiredName(BodyColumns::BodyType);

        bool bHasValidNumbers = true;
        bHasValidNumbers &= BodyColumns::ReadRequiredFloat(Row, BodyColumns::SegmentMaxHP, NewRow.SegmentMaxHP, Report, Index, ParsedRowName);
        bHasValidNumbers &= BodyColumns::ReadRequiredFloat(Row, BodyColumns::MaxStamina, NewRow.MaxStamina, Report, Index, ParsedRowName);
        bHasValidNumbers &= BodyColumns::ReadRequiredFloat(Row, BodyColumns::StaminaRegen, NewRow.StaminaRegen, Report, Index, ParsedRowName, true);
        bHasValidNumbers &= BodyColumns::ReadRequiredFloat(Row, BodyColumns::SegmentMass, NewRow.SegmentMass, Report, Index, ParsedRowName);
        bHasValidNumbers &= BodyColumns::ReadRequiredFloat(Row, BodyColumns::GroundFriction, NewRow.GroundFriction, Report, Index, ParsedRowName, true);
        bHasValidNumbers &= BodyColumns::ReadRequiredFloat(Row, BodyColumns::LinearDamping, NewRow.LinearDamping, Report, Index, ParsedRowName, true);
        bHasValidNumbers &= BodyColumns::ReadRequiredFloat(Row, BodyColumns::AngularDamping, NewRow.AngularDamping, Report, Index, ParsedRowName, true);
        bHasValidNumbers &= BodyColumns::ReadRequiredFloat(Row, BodyColumns::MaxVelocity, NewRow.MaxVelocity, Report, Index, ParsedRowName);

        if (!Row.IsValid() || !bHasValidNumbers)
        {
            continue;
        }

        // RowName은 NewRow의 멤버가 아니라 DataTable 자체의 행 키로 저장된다.
        TargetTable->AddRow(ParsedRowName, NewRow);
        Report.AddSuccess();

        UE_LOG(LogChimeraBodyDataParser, Verbose,
            TEXT("[CSV -> DataTable] Row=%s ID=%s Type=%s SegmentHP=%.1f MaxStamina=%.1f"),
            *ParsedRowName.ToString(),
            *NewRow.ID.ToString(),
            *NewRow.BodyType.ToString(),
            NewRow.SegmentMaxHP,
            NewRow.MaxStamina);
    }

    return FinalizeParseReport(ParserName, Report, OutError);
}
