// Fill out your copyright notice in the Description page of Project Settings.


#include "BodyDataParser.h"
#include "Data/Body/CMBodyTableRow.h"
#include "Parser/SheetDataTableUtils.h"
#include "Parser/SheetParserUtils.h"
#include "Parser/SheetValidation.h"

namespace BodyColumns
{
    const FString RowName = TEXT("RowName");
    const FString ID = TEXT("ID");
    const FString BodyType = TEXT("BodyType");

    const TArray<FString> RequiredHeaders =
    {
        ID,
        BodyType,
    };
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
        
        NewRow.RowName = Row.GetRequiredName(BodyColumns::RowName);
        NewRow.ID = Row.Get(BodyColumns::ID);
        NewRow.BodyType = Row.Get(BodyColumns::BodyType);

        if (!Row.IsValid())
        {
            continue;
        }

        TargetTable->AddRow(NewRow.RowName, NewRow);
        Report.AddSuccess();
    }

    return FinalizeParseReport(ParserName, Report, OutError);
}