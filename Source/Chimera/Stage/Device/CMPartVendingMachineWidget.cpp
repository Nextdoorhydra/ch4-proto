#include "Stage/Device/CMPartVendingMachineWidget.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UCMPartVendingMachineWidget::RebuildWidget()
{
    IconBrush.DrawAs = ESlateBrushDrawType::Image;
    IconBrush.ImageSize = FVector2D(88.0f, 88.0f);

    TSharedRef<SWidget> Root =
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Center)
        [
            SNew(SBox)
            .WidthOverride(92.0f)
            .HeightOverride(92.0f)
            [
                SAssignNew(IconImage, SImage)
                .Image(&IconBrush)
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Center)
        .Padding(0.0f, 4.0f, 0.0f, 0.0f)
        [
            SAssignNew(RemainingUsesText, STextBlock)
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 26))
            .ColorAndOpacity(FLinearColor::White)
            .ShadowOffset(FVector2D(2.0f, 2.0f))
            .ShadowColorAndOpacity(FLinearColor::Black)
        ];

    ApplyPresentation();
    return Root;
}

void UCMPartVendingMachineWidget::SetPresentation(
    UTexture2D* InPartIcon,
    int32 InRemainingUses)
{
    PartIcon = InPartIcon;
    RemainingUses = InRemainingUses;
    ApplyPresentation();
}

void UCMPartVendingMachineWidget::ApplyPresentation()
{
    IconBrush.SetResourceObject(PartIcon);
    if (IconImage)
    {
        IconImage->SetImage(&IconBrush);
    }
    if (RemainingUsesText)
    {
        RemainingUsesText->SetText(FText::Format(
            NSLOCTEXT("CMPartVendingMachine", "RemainingUses", "x{0}"),
            FText::AsNumber(RemainingUses)));
    }
}
