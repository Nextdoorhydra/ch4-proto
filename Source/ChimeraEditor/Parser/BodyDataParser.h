// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GoogleSheetParserBase.h"
#include "BodyDataParser.generated.h"

class UDataTable;

UCLASS(EditInlineNew)
class CHIMERAEDITOR_API UBodyDataParser : public UGoogleSheetParserBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Output")
	TObjectPtr<UDataTable> TargetTable;

protected:
	virtual bool OnParseComplete(FString& OutError) override;
};
