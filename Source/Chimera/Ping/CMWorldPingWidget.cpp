#include "Ping/CMWorldPingWidget.h"

#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UCMWorldPingWidget::RebuildWidget()
{
    IconBrush.DrawAs = ESlateBrushDrawType::Image;
    IconBrush.ImageSize = FVector2D(92.0f, 92.0f);

    TSharedRef<SWidget> Root =
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Center)
        .Padding(0.0f, 0.0f, 0.0f, 4.0f)
        [
            SAssignNew(PlayerNameText, STextBlock)
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
            .ShadowOffset(FVector2D(1.5f, 1.5f))
            .ShadowColorAndOpacity(FLinearColor::Black)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Center)
        [
            SNew(SBox)
            .WidthOverride(96.0f)
            .HeightOverride(96.0f)
            [
                SAssignNew(IconImage, SImage)
                .Image(&IconBrush)
            ]
        ];

    ApplyPresentation();
    return Root;
}

void UCMWorldPingWidget::SetPresentation(
    ECMPingType Type,
    const FString& PlayerName,
    const FLinearColor& PlayerColor)
{
    PingType = Type;
    PingingPlayerName = PlayerName;
    PingingPlayerColor = PlayerColor;
    ApplyPresentation();
}

void UCMWorldPingWidget::ApplyPresentation()
{
    IconBrush.SetResourceObject(LoadIcon());
    if (IconImage)
    {
        IconImage->SetImage(&IconBrush);
    }
    if (PlayerNameText)
    {
        PlayerNameText->SetText(FText::FromString(PingingPlayerName));
        PlayerNameText->SetColorAndOpacity(PingingPlayerColor);
    }
}

UObject* UCMWorldPingWidget::LoadIcon() const
{
    const TCHAR* AssetPath = nullptr;
    switch (PingType)
    {
    case ECMPingType::GoHere:
        AssetPath = TEXT("/Game/Chimera/UI/Ping/T_Ping_GoHere.T_Ping_GoHere");
        break;
    case ECMPingType::LookHere:
        AssetPath = TEXT("/Game/Chimera/UI/Ping/T_Ping_LookHere.T_Ping_LookHere");
        break;
    case ECMPingType::SwapParts:
        AssetPath = TEXT("/Game/Chimera/UI/Ping/T_Ping_SwapParts.T_Ping_SwapParts");
        break;
    }
    return AssetPath ? LoadObject<UTexture2D>(nullptr, AssetPath) : nullptr;
}
