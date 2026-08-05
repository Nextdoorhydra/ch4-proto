#include "ChimeraLobbyPlayerRowWidget.h"

#include "Components/TextBlock.h"

void UChimeraLobbyPlayerRowWidget::SetPlayerDisplayName(
    const FString& NewPlayerDisplayName,
    const FLinearColor& NewPlayerDisplayColor
)
{
    PlayerDisplayName = FText::FromString(NewPlayerDisplayName);
    PlayerDisplayColor = NewPlayerDisplayColor;
    if (Txt_PlayerName)
    {
        Txt_PlayerName->SetText(PlayerDisplayName);
        Txt_PlayerName->SetColorAndOpacity(PlayerDisplayColor);
    }
}

void UChimeraLobbyPlayerRowWidget::NativePreConstruct()
{
    Super::NativePreConstruct();

    SetClipping(EWidgetClipping::ClipToBounds);

    if (Txt_PlayerName)
    {
        Txt_PlayerName->SetAutoWrapText(false);
        Txt_PlayerName->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
        Txt_PlayerName->SetClipping(EWidgetClipping::ClipToBounds);
        Txt_PlayerName->SetColorAndOpacity(PlayerDisplayColor);

        if (!PlayerDisplayName.IsEmpty())
        {
            Txt_PlayerName->SetText(PlayerDisplayName);
        }
    }
}
