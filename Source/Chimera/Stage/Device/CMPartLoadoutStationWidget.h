#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "CMPartLoadoutStationWidget.generated.h"

class ACMPartLoadoutStation;
class STextBlock;

/** 파츠 구성 저장 장치의 호스트 전용 10슬롯 UI다. */
UCLASS()
class CHIMERA_API UCMPartLoadoutStationWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void InitializeStation(ACMPartLoadoutStation* InStation);
    void RefreshSlots();

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    FReply HandleSaveClicked(int32 SlotIndex);
    FReply HandleLoadClicked(int32 SlotIndex);

    TWeakObjectPtr<ACMPartLoadoutStation> Station;
    TArray<TSharedPtr<STextBlock>> SlotStatusTexts;
};
