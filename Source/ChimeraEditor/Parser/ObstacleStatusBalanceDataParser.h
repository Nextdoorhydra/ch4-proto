#pragma once

#include "CoreMinimal.h"
#include "GoogleSheetParserBase.h"
#include "ObstacleStatusBalanceDataParser.generated.h"

class UDataTable;

UCLASS(EditInlineNew)
class CHIMERAEDITOR_API UObstacleStatusBalanceDataParser
    : public UGoogleSheetParserBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category="Output") TObjectPtr<UDataTable> TargetTable;

protected:
    virtual bool OnParseComplete(FString& OutError) override;
};
