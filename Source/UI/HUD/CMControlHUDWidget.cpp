#include "HUD/CMControlHUDWidget.h"

#include "Player/CMChimera.h"
#include "Player/CMControlBody.h"
#include "Player/CMPlayerState.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#define LOCTEXT_NAMESPACE "CMControlHUDWidget"

UCMControlHUDWidget::UCMControlHUDWidget(
    const FObjectInitializer& ObjectInitializer
)
    : Super(ObjectInitializer)
{
    // Chimera HUD는 Q/W/E/R 게임 입력과 마우스 포인터를 동시에 사용한다.
    InputConfig = ENKMUIWidgetInputMode::GameAndMenu;
    GameMouseCaptureMode = EMouseCaptureMode::NoCapture;
}

void UCMControlHUDWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (!WidgetTree->RootWidget)
    {
        BuildFallbackWidgetTree();
    }

    if (!CacheWidgetTreeReferences())
    {
        UE_LOG(LogTemp, Error,
            TEXT("WBP_CMControlHUD requires ControlSlotBox, ControlSlotBorder0-3, and AssignmentText0-3."));
        return;
    }

    SetVisibility(ESlateVisibility::HitTestInvisible);
    ControlSlotBox->SetVisibility(ESlateVisibility::Collapsed);
}

void UCMControlHUDWidget::BuildFallbackWidgetTree()
{
    UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
        UCanvasPanel::StaticClass(),
        TEXT("RootCanvas")
    );
    WidgetTree->RootWidget = RootCanvas;

    ControlSlotBox = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(),
        TEXT("ControlSlotBox")
    );
    UCanvasPanelSlot* ControlSlotCanvas =
        RootCanvas->AddChildToCanvas(ControlSlotBox);
    ControlSlotCanvas->SetAnchors(FAnchors(0.5f, 1.0f));
    ControlSlotCanvas->SetAlignment(FVector2D(0.5f, 1.0f));
    ControlSlotCanvas->SetPosition(HUDPosition);
    ControlSlotCanvas->SetAutoSize(true);

    static const TCHAR* KeyLabels[] =
    {
        TEXT("Q"),
        TEXT("W"),
        TEXT("E"),
        TEXT("R")
    };

    for (int32 SlotIndex = 0;
        SlotIndex < CMControl::MaxKeysPerPlayer;
        ++SlotIndex)
    {
        USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(
            USizeBox::StaticClass(),
            FName(*FString::Printf(TEXT("ControlSlotSize%d"), SlotIndex))
        );
        CardSize->SetWidthOverride(CardWidth);
        CardSize->SetHeightOverride(CardHeight);

        UHorizontalBoxSlot* CardSlot =
            ControlSlotBox->AddChildToHorizontalBox(CardSize);
        CardSlot->SetPadding(FMargin(CardSpacing, 0.0f));

        UBorder* CardBorder = WidgetTree->ConstructWidget<UBorder>(
            UBorder::StaticClass(),
            FName(*FString::Printf(TEXT("ControlSlotBorder%d"), SlotIndex))
        );
        CardBorder->SetPadding(FMargin(10.0f, 7.0f));
        CardSize->SetContent(CardBorder);

        UVerticalBox* TextBox = WidgetTree->ConstructWidget<UVerticalBox>(
            UVerticalBox::StaticClass(),
            FName(*FString::Printf(TEXT("ControlSlotTextBox%d"), SlotIndex))
        );
        CardBorder->SetContent(TextBox);

        UTextBlock* KeyText = WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(),
            FName(*FString::Printf(TEXT("KeyText%d"), SlotIndex))
        );
        KeyText->SetText(FText::FromString(KeyLabels[SlotIndex]));
        KeyText->SetJustification(ETextJustify::Center);
        KeyText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        FSlateFontInfo KeyFont = KeyText->GetFont();
        KeyFont.Size = KeyFontSize;
        KeyText->SetFont(KeyFont);
        UVerticalBoxSlot* KeySlot = TextBox->AddChildToVerticalBox(KeyText);
        KeySlot->SetHorizontalAlignment(HAlign_Fill);

        UTextBlock* AssignmentText =
            WidgetTree->ConstructWidget<UTextBlock>(
                UTextBlock::StaticClass(),
                FName(*FString::Printf(TEXT("AssignmentText%d"), SlotIndex))
            );
        AssignmentText->SetText(LOCTEXT("Unassigned", "미배정"));
        AssignmentText->SetJustification(ETextJustify::Center);
        AssignmentText->SetColorAndOpacity(
            FSlateColor(FLinearColor::White));
        FSlateFontInfo AssignmentFont = AssignmentText->GetFont();
        AssignmentFont.Size = AssignmentFontSize;
        AssignmentText->SetFont(AssignmentFont);
        UVerticalBoxSlot* AssignmentSlot =
            TextBox->AddChildToVerticalBox(AssignmentText);
        AssignmentSlot->SetHorizontalAlignment(HAlign_Fill);
    }
}

bool UCMControlHUDWidget::CacheWidgetTreeReferences()
{
    ControlSlotBox = Cast<UHorizontalBox>(
        WidgetTree->FindWidget(TEXT("ControlSlotBox"))
    );
    ControlSlotBorders.Reset();
    AssignmentTexts.Reset();

    for (int32 SlotIndex = 0;
        SlotIndex < CMControl::MaxKeysPerPlayer;
        ++SlotIndex)
    {
        UBorder* CardBorder = Cast<UBorder>(WidgetTree->FindWidget(
            FName(*FString::Printf(TEXT("ControlSlotBorder%d"), SlotIndex))
        ));
        UTextBlock* AssignmentText = Cast<UTextBlock>(WidgetTree->FindWidget(
            FName(*FString::Printf(TEXT("AssignmentText%d"), SlotIndex))
        ));
        if (!CardBorder || !AssignmentText)
        {
            return false;
        }
        ControlSlotBorders.Add(CardBorder);
        AssignmentTexts.Add(AssignmentText);
    }

    return ControlSlotBox != nullptr;
}

void UCMControlHUDWidget::NativeTick(
    const FGeometry& MyGeometry,
    float InDeltaTime
)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshControlSlots();
}

void UCMControlHUDWidget::SetControlBody(
    ACMControlBody* NewControlBody
)
{
    ControlBody = NewControlBody;
    RefreshControlSlots();
}

void UCMControlHUDWidget::RefreshControlSlots()
{
    if (!ControlSlotBox
        || ControlSlotBorders.Num() != CMControl::MaxKeysPerPlayer
        || AssignmentTexts.Num() != CMControl::MaxKeysPerPlayer)
    {
        return;
    }

    const ACMControlBody* CurrentControlBody = ControlBody.Get();
    const ACMChimera* SharedChimera = CurrentControlBody
        ? CurrentControlBody->GetSharedChimera()
        : nullptr;
    if (!CurrentControlBody || !SharedChimera)
    {
        ControlSlotBox->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    ControlSlotBox->SetVisibility(ESlateVisibility::HitTestInvisible);
    const ACMPlayerState* PlayerState = GetOwningPlayer()
        ? GetOwningPlayer()->GetPlayerState<ACMPlayerState>()
        : nullptr;
    const FLinearColor PlayerColor = PlayerState
        ? PlayerState->GetPlayerColor()
        : FLinearColor::White;

    for (int32 SlotIndex = 0;
        SlotIndex < CMControl::MaxKeysPerPlayer;
        ++SlotIndex)
    {
        const FCMPartSlotAddress SlotAddress =
            CurrentControlBody->GetPartSlotAddressForControlSlot(SlotIndex);
        const bool bAssigned = CMControl::IsValidPartSlot(SlotAddress);
        const bool bEnabled = bAssigned
            && CurrentControlBody->IsControlSlotEnabled(SlotIndex);
        const bool bPressed = bEnabled
            && SharedChimera->IsPartSlotPressed(SlotAddress);

        FLinearColor CardColor = DisabledCardColor;
        if (bEnabled)
        {
            CardColor = PlayerColor;
            CardColor.A = bPressed ? PressedOpacity : EnabledOpacity;
        }
        ControlSlotBorders[SlotIndex]->SetBrushColor(CardColor);

        if (!bAssigned)
        {
            AssignmentTexts[SlotIndex]->SetText(
                LOCTEXT("Unassigned", "미배정"));
            continue;
        }

        const FText SideText = CMControl::IsLeftPartSlot(SlotAddress)
            ? LOCTEXT("LeftSide", "왼쪽")
            : LOCTEXT("RightSide", "오른쪽");
        AssignmentTexts[SlotIndex]->SetText(FText::Format(
            LOCTEXT("AssignmentFormat", "몸통 {0} · {1}"),
            FText::AsNumber(SlotAddress.SegmentIndex + 1),
            SideText
        ));
    }
}

#undef LOCTEXT_NAMESPACE
