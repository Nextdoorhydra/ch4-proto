#include "CMLobbyPlayerRowWidget.h"

#include "Components/TextBlock.h"

void UCMLobbyPlayerRowWidget::SetPlayerDisplayName(
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

// 별도 Ready 텍스트가 있으면 옆에 표시하고 없으면 이름 텍스트에 함께 표시
void UCMLobbyPlayerRowWidget::SetPlayerLobbyState(
    const FString& NewPlayerDisplayName,
    const FLinearColor& NewPlayerDisplayColor,
    bool bNewReady
)
{
    PlayerDisplayName = FText::FromString(NewPlayerDisplayName);
    PlayerDisplayColor = NewPlayerDisplayColor;
    bReady = bNewReady;

    const FText ReadyText =
        NSLOCTEXT("ChimeraUI", "LobbyPlayerReady", "Ready");
    if (Txt_ReadyState)
    {
        Txt_ReadyState->SetText(ReadyText);
        Txt_ReadyState->SetColorAndOpacity(
            bReady ? FLinearColor::Green : FLinearColor::Red);
    }
    if (Txt_PlayerName)
    {
        Txt_PlayerName->SetText(PlayerDisplayName);
        Txt_PlayerName->SetColorAndOpacity(PlayerDisplayColor);
    }
}

void UCMLobbyPlayerRowWidget::NativePreConstruct()
{
    Super::NativePreConstruct();

    SetClipping(EWidgetClipping::ClipToBounds);

    if (Txt_PlayerName)
    {
        Txt_PlayerName->SetAutoWrapText(false);
        Txt_PlayerName->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
        Txt_PlayerName->SetClipping(EWidgetClipping::ClipToBounds);
        Txt_PlayerName->SetColorAndOpacity(PlayerDisplayColor);

    }

    if (Txt_ReadyState)
    {
        Txt_ReadyState->SetAutoWrapText(false);
        Txt_ReadyState->SetText(
            NSLOCTEXT("ChimeraUI", "LobbyPlayerReady", "Ready"));
    }

    SetPlayerLobbyState(
        PlayerDisplayName.ToString(),
        PlayerDisplayColor,
        bReady);
}
