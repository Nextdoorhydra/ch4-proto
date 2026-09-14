#include "Stage/Device/CMPartLoadoutStationWidget.h"

#include "Stage/Device/CMPartLoadoutStation.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UCMPartLoadoutStationWidget::RebuildWidget()
{
    SlotStatusTexts.Reset();
    TSharedRef<SVerticalBox> SlotList = SNew(SVerticalBox);
    const int32 SlotCount = Station.IsValid()
        ? Station->GetStorageSlotCount()
        : 10;
    for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
    {
        TSharedPtr<STextBlock> StatusText;
        SlotList->AddSlot()
        .AutoHeight()
        .Padding(4.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SAssignNew(StatusText, STextBlock)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(6.0f, 0.0f)
            [
                SNew(SButton)
                .Text(NSLOCTEXT("CMPartLoadoutStation", "Save", "Save"))
                .OnClicked_Lambda([this, SlotIndex]()
                {
                    return HandleSaveClicked(SlotIndex);
                })
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SButton)
                .Text(NSLOCTEXT("CMPartLoadoutStation", "Load", "Load"))
                .IsEnabled_Lambda([this, SlotIndex]()
                {
                    return Station.IsValid()
                        && Station->IsStorageSlotOccupied(SlotIndex);
                })
                .OnClicked_Lambda([this, SlotIndex]()
                {
                    return HandleLoadClicked(SlotIndex);
                })
            ]
        ];
        SlotStatusTexts.Add(StatusText);
    }

    TSharedRef<SWidget> Root =
        SNew(SOverlay)
        + SOverlay::Slot()
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            SNew(SBox)
            .WidthOverride(520.0f)
            .HeightOverride(640.0f)
            [
                SNew(SBorder)
                .Padding(20.0f)
                .BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.025f, 0.94f))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .HAlign(HAlign_Center)
                    .Padding(0.0f, 0.0f, 0.0f, 16.0f)
                    [
                        SNew(STextBlock)
                        .Text(NSLOCTEXT(
                            "CMPartLoadoutStation", "Title", "PART LOADOUT"))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 28))
                    ]
                    + SVerticalBox::Slot()
                    .FillHeight(1.0f)
                    [
                        SNew(SScrollBox)
                        + SScrollBox::Slot()
                        [
                            SlotList
                        ]
                    ]
                ]
            ]
        ];

    RefreshSlots();
    return Root;
}

void UCMPartLoadoutStationWidget::InitializeStation(
    ACMPartLoadoutStation* InStation)
{
    Station = InStation;
    RefreshSlots();
}

void UCMPartLoadoutStationWidget::RefreshSlots()
{
    for (int32 SlotIndex = 0; SlotIndex < SlotStatusTexts.Num(); ++SlotIndex)
    {
        if (!SlotStatusTexts[SlotIndex])
        {
            continue;
        }

        const bool bOccupied = Station.IsValid()
            && Station->IsStorageSlotOccupied(SlotIndex);
        const int32 PartCount = bOccupied
            ? Station->GetStoredPartCount(SlotIndex)
            : 0;
        SlotStatusTexts[SlotIndex]->SetText(bOccupied
            ? FText::Format(
                NSLOCTEXT("CMPartLoadoutStation", "SavedSlot",
                    "Slot {0}  -  Saved ({1} Parts)"),
                FText::AsNumber(SlotIndex + 1),
                FText::AsNumber(PartCount))
            : FText::Format(
                NSLOCTEXT("CMPartLoadoutStation", "EmptySlot",
                    "Slot {0}  -  Empty"),
                FText::AsNumber(SlotIndex + 1)));
    }
}

FReply UCMPartLoadoutStationWidget::HandleSaveClicked(int32 SlotIndex)
{
    if (Station.IsValid())
    {
        Station->SaveCurrentLoadout(SlotIndex);
        RefreshSlots();
    }
    return FReply::Handled();
}

FReply UCMPartLoadoutStationWidget::HandleLoadClicked(int32 SlotIndex)
{
    if (Station.IsValid())
    {
        Station->LoadSavedLoadout(SlotIndex);
        RefreshSlots();
    }
    return FReply::Handled();
}
