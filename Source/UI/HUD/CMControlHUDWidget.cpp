#include "HUD/CMControlHUDWidget.h"

#include "Player/CMChimera.h"
#include "Player/CMControlBody.h"
#include "Player/CMPartSlotComponent.h"
#include "Player/CMPlayerState.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#define LOCTEXT_NAMESPACE "CMControlHUDWidget"

namespace
{
float GetResourcePercent(const float Current, const float Maximum)
{
    return Maximum > 0.0f
        ? FMath::Clamp(Current / Maximum, 0.0f, 1.0f)
        : 0.0f;
}
}

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
            TEXT("WBP_CMControlHUD is missing its stamina, body health, or control-slot widgets."));
        return;
    }

    SetVisibility(ESlateVisibility::HitTestInvisible);
    HUDContainer->SetVisibility(ESlateVisibility::Collapsed);
}

void UCMControlHUDWidget::BuildFallbackWidgetTree()
{
    UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
        UCanvasPanel::StaticClass(),
        TEXT("RootCanvas")
    );
    WidgetTree->RootWidget = RootCanvas;

    HUDContainer = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(),
        TEXT("HUDContainer")
    );
    UCanvasPanelSlot* ControlSlotCanvas =
        RootCanvas->AddChildToCanvas(HUDContainer);
    ControlSlotCanvas->SetAnchors(FAnchors(0.5f, 1.0f));
    ControlSlotCanvas->SetAlignment(FVector2D(0.5f, 1.0f));
    ControlSlotCanvas->SetPosition(HUDPosition);
    ControlSlotCanvas->SetAutoSize(true);

    USizeBox* StaminaSize = WidgetTree->ConstructWidget<USizeBox>(
        USizeBox::StaticClass(), TEXT("StaminaSize"));
    StaminaSize->SetWidthOverride(420.0f);
    StaminaSize->SetHeightOverride(18.0f);
    HUDContainer->AddChildToVerticalBox(StaminaSize);
    UOverlay* StaminaOverlay = WidgetTree->ConstructWidget<UOverlay>(
        UOverlay::StaticClass(), TEXT("StaminaOverlay"));
    StaminaSize->SetContent(StaminaOverlay);
    StaminaProgressBar = WidgetTree->ConstructWidget<UProgressBar>(
        UProgressBar::StaticClass(), TEXT("StaminaProgressBar"));
    StaminaProgressBar->SetPercent(1.0f);
    StaminaProgressBar->SetFillColorAndOpacity(
        FLinearColor(0.05f, 0.7f, 0.85f, 1.0f));
    StaminaOverlay->AddChildToOverlay(StaminaProgressBar);
    UTextBlock* StaminaLabel = WidgetTree->ConstructWidget<UTextBlock>(
        UTextBlock::StaticClass(), TEXT("StaminaLabel"));
    StaminaLabel->SetText(LOCTEXT("SharedStamina", "공용 스태미너"));
    StaminaLabel->SetJustification(ETextJustify::Center);
    UOverlaySlot* StaminaLabelSlot =
        StaminaOverlay->AddChildToOverlay(StaminaLabel);
    StaminaLabelSlot->SetHorizontalAlignment(HAlign_Fill);
    StaminaLabelSlot->SetVerticalAlignment(VAlign_Center);

    UHorizontalBox* BodyHealthRow =
        WidgetTree->ConstructWidget<UHorizontalBox>(
            UHorizontalBox::StaticClass(), TEXT("BodyHealthRow"));
    HUDContainer->AddChildToVerticalBox(BodyHealthRow);
    for (int32 BodyIndex = 0; BodyIndex < 2; ++BodyIndex)
    {
        USizeBox* BodyHealthSize = WidgetTree->ConstructWidget<USizeBox>(
            USizeBox::StaticClass(),
            FName(*FString::Printf(TEXT("BodyHealthSize%d"), BodyIndex)));
        BodyHealthSize->SetWidthOverride((CardWidth + CardSpacing * 2.0f) * 2.0f);
        BodyHealthSize->SetHeightOverride(18.0f);
        UHorizontalBoxSlot* BodySlot =
            BodyHealthRow->AddChildToHorizontalBox(BodyHealthSize);
        BodySlot->SetPadding(FMargin(CardSpacing, 4.0f));

        UOverlay* BodyHealthOverlay =
            WidgetTree->ConstructWidget<UOverlay>(
                UOverlay::StaticClass(),
                FName(*FString::Printf(
                    TEXT("BodyHealthOverlay%d"), BodyIndex)));
        BodyHealthSize->SetContent(BodyHealthOverlay);
        UProgressBar* BodyHealthBar =
            WidgetTree->ConstructWidget<UProgressBar>(
                UProgressBar::StaticClass(),
                FName(*FString::Printf(TEXT("BodyHealthBar%d"), BodyIndex)));
        BodyHealthBar->SetPercent(1.0f);
        BodyHealthBar->SetFillColorAndOpacity(
            FLinearColor(0.75f, 0.04f, 0.04f, 1.0f));
        BodyHealthOverlay->AddChildToOverlay(BodyHealthBar);
        UTextBlock* BodyHealthLabel = WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(),
            FName(*FString::Printf(TEXT("BodyHealthLabel%d"), BodyIndex)));
        BodyHealthLabel->SetText(BodyIndex == 0
            ? LOCTEXT("QWBody", "Q/W 몸통")
            : LOCTEXT("ERBody", "E/R 몸통"));
        BodyHealthLabel->SetJustification(ETextJustify::Center);
        UOverlaySlot* BodyLabelSlot =
            BodyHealthOverlay->AddChildToOverlay(BodyHealthLabel);
        BodyLabelSlot->SetHorizontalAlignment(HAlign_Fill);
        BodyLabelSlot->SetVerticalAlignment(VAlign_Center);
    }

    ControlSlotBox = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("ControlSlotBox"));
    HUDContainer->AddChildToVerticalBox(ControlSlotBox);

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
        CardBorder->SetPadding(FMargin(0.0f));
        CardSize->SetContent(CardBorder);

        UOverlay* CardOverlay = WidgetTree->ConstructWidget<UOverlay>(
            UOverlay::StaticClass(),
            FName(*FString::Printf(TEXT("ControlSlotOverlay%d"), SlotIndex))
        );
        CardBorder->SetContent(CardOverlay);

        UProgressBar* PartHealthBar =
            WidgetTree->ConstructWidget<UProgressBar>(
                UProgressBar::StaticClass(),
                FName(*FString::Printf(TEXT("PartHealthBar%d"), SlotIndex)));
        PartHealthBar->SetPercent(0.0f);
        PartHealthBar->SetFillColorAndOpacity(
            FLinearColor(0.85f, 0.03f, 0.03f, 0.72f));
        FProgressBarStyle PartHealthStyle =
            PartHealthBar->GetWidgetStyle();
        PartHealthStyle.BackgroundImage.TintColor =
            FSlateColor(FLinearColor::Transparent);
        PartHealthBar->SetWidgetStyle(PartHealthStyle);
        CardOverlay->AddChildToOverlay(PartHealthBar);

        UVerticalBox* TextBox = WidgetTree->ConstructWidget<UVerticalBox>(
            UVerticalBox::StaticClass(),
            FName(*FString::Printf(TEXT("ControlSlotTextBox%d"), SlotIndex))
        );
        UOverlaySlot* TextSlot = CardOverlay->AddChildToOverlay(TextBox);
        TextSlot->SetHorizontalAlignment(HAlign_Fill);
        TextSlot->SetVerticalAlignment(VAlign_Center);

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
    HUDContainer = Cast<UVerticalBox>(
        WidgetTree->FindWidget(TEXT("HUDContainer"))
    );
    ControlSlotBox = Cast<UHorizontalBox>(
        WidgetTree->FindWidget(TEXT("ControlSlotBox"))
    );
    StaminaProgressBar = Cast<UProgressBar>(
        WidgetTree->FindWidget(TEXT("StaminaProgressBar"))
    );
    BodyHealthBars.Reset();
    PartHealthBars.Reset();
    ControlSlotBorders.Reset();
    AssignmentTexts.Reset();

    for (int32 SlotIndex = 0;
        SlotIndex < CMControl::MaxKeysPerPlayer;
        ++SlotIndex)
    {
        UProgressBar* PartHealthBar = Cast<UProgressBar>(
            WidgetTree->FindWidget(FName(*FString::Printf(
                TEXT("PartHealthBar%d"), SlotIndex)))
        );
        UBorder* CardBorder = Cast<UBorder>(WidgetTree->FindWidget(
            FName(*FString::Printf(TEXT("ControlSlotBorder%d"), SlotIndex))
        ));
        UTextBlock* AssignmentText = Cast<UTextBlock>(WidgetTree->FindWidget(
            FName(*FString::Printf(TEXT("AssignmentText%d"), SlotIndex))
        ));
        if (!PartHealthBar || !CardBorder || !AssignmentText)
        {
            return false;
        }
        PartHealthBars.Add(PartHealthBar);
        ControlSlotBorders.Add(CardBorder);
        AssignmentTexts.Add(AssignmentText);
    }

    for (int32 BodyIndex = 0; BodyIndex < 2; ++BodyIndex)
    {
        UProgressBar* BodyHealthBar = Cast<UProgressBar>(
            WidgetTree->FindWidget(FName(*FString::Printf(
                TEXT("BodyHealthBar%d"), BodyIndex)))
        );
        if (!BodyHealthBar)
        {
            return false;
        }
        BodyHealthBars.Add(BodyHealthBar);
    }

    return HUDContainer && ControlSlotBox && StaminaProgressBar;
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
    if (!HUDContainer || !ControlSlotBox || !StaminaProgressBar
        || BodyHealthBars.Num() != 2
        || PartHealthBars.Num() != CMControl::MaxKeysPerPlayer
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
        HUDContainer->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    HUDContainer->SetVisibility(ESlateVisibility::HitTestInvisible);
    StaminaProgressBar->SetPercent(GetResourcePercent(
        SharedChimera->GetStamina(),
        SharedChimera->GetMaxStamina()
    ));

    const TArray<FCMBodySegmentHealthState> SegmentStates =
        SharedChimera->GetSegmentHealthStates();
    const int32 OwnedSegmentIndex =
        CurrentControlBody->GetOwnedSegmentIndex();
    for (int32 BodyIndex = 0; BodyIndex < 2; ++BodyIndex)
    {
        const int32 SegmentIndex = OwnedSegmentIndex + BodyIndex;
        const FCMBodySegmentHealthState* SegmentState =
            SegmentStates.IsValidIndex(SegmentIndex)
                ? &SegmentStates[SegmentIndex]
                : nullptr;
        BodyHealthBars[BodyIndex]->SetPercent(SegmentState
            ? GetResourcePercent(
                SegmentState->Health, SegmentState->MaxHealth)
            : 0.0f);
    }
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

        const UCMPartSlotComponent* PartSlot = bAssigned
            ? SharedChimera->GetPartSlotComponent(SlotAddress)
            : nullptr;
        const ACMPartActorBase* PartActor = PartSlot
            ? Cast<ACMPartActorBase>(PartSlot->GetAttachedPart())
            : nullptr;
        PartHealthBars[SlotIndex]->SetPercent(PartActor
            ? GetResourcePercent(
                PartActor->GetHealth(), PartActor->GetMaxHealth())
            : 0.0f);

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
