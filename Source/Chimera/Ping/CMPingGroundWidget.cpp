#include "Ping/CMPingGroundWidget.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"

TSharedRef<SWidget> UCMPingGroundWidget::RebuildWidget()
{
    CircleBrush = *FCoreStyle::Get().GetBrush("WhiteBrush");
    CircleBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
    CircleBrush.OutlineSettings.CornerRadii = FVector4(128.0f);
    CircleBrush.OutlineSettings.Width = 10.0f;
    CircleBrush.OutlineSettings.bUseBrushTransparency = false;

    TSharedRef<SWidget> Root =
        SNew(SBox)
        .WidthOverride(256.0f)
        .HeightOverride(256.0f)
        [
            SAssignNew(CircleImage, SImage)
            .Image(&CircleBrush)
        ];

    ApplyPresentation();
    return Root;
}

void UCMPingGroundWidget::SetPingType(ECMPingType Type)
{
    PingType = Type;
    ApplyPresentation();
}

void UCMPingGroundWidget::ApplyPresentation()
{
    const FLinearColor Color = CMPing::GetTypeColor(PingType);
    CircleBrush.TintColor = FLinearColor::Transparent;
    CircleBrush.OutlineSettings.Color = Color.CopyWithNewOpacity(0.95f);
    if (CircleImage)
    {
        CircleImage->SetImage(&CircleBrush);
    }
}
