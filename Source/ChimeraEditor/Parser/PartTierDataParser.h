#pragma once

#include "CoreMinimal.h"
#include "GoogleSheetParserBase.h"

#include "PartTierDataParser.generated.h"

class UDataTable;

/** Converts the shared Part tier sheet into a validated DataTable. */
UCLASS(EditInlineNew)
class CHIMERAEDITOR_API UPartTierDataParser
    : public UGoogleSheetParserBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Output")
    TObjectPtr<UDataTable> TargetTable;

protected:
    virtual bool OnParseComplete(FString& OutError) override;
};
