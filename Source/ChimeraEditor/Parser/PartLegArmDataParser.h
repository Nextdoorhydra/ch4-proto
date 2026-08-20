#pragma once

#include "CoreMinimal.h"
#include "GoogleSheetParserBase.h"

#include "PartLegArmDataParser.generated.h"

class UDataTable;

/** Converts the combined Arm/Leg Google sheet into a validated DataTable. */
UCLASS(EditInlineNew)
class CHIMERAEDITOR_API UPartLegArmDataParser
    : public UGoogleSheetParserBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Output")
    TObjectPtr<UDataTable> TargetTable;

protected:
    virtual bool OnParseComplete(FString& OutError) override;
};
