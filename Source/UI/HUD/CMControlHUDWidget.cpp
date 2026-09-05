#include "HUD/CMControlHUDWidget.h"

#include "Ability/CMChimeraAttributeSet.h"
#include "GameMode/CMGameState.h"
#include "HUD/Wireframe/CMWireframeHUDCaptureActor.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Parts/Core/CMPartStatusComponent.h"
#include "Parts/Core/CMPartStatusTags.h"
#include "Player/CMChimera.h"
#include "Player/CMControlBody.h"
#include "Player/CMPartInterface.h"
#include "Player/CMPartSlotComponent.h"
#include "Player/CMPlayerState.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BoxComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Curves/CurveLinearColor.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "CMControlHUDWidget"

namespace
{
const FLinearColor IdleControlKeyColor(0.55f, 0.55f, 0.55f, 0.65f);
const FLinearColor DisabledControlKeyColor(0.22f, 0.22f, 0.22f, 0.45f);
const FLinearColor DeadSegmentColor(0.10f, 0.10f, 0.10f, 0.95f);
constexpr float StatusCycleSeconds = 1.6f;
constexpr float StatusNameEnd = 0.62f;
constexpr float StatusTextStart = 0.80f;
constexpr float StatusTextEnd = 1.42f;

float GetPercent(float Current, float Maximum)
{
    return Maximum > 0.0f
        ? FMath::Clamp(Current / Maximum, 0.0f, 1.0f)
        : 0.0f;
}

FSlateBrush MakeSolidBrush(const FLinearColor& Color)
{
    FSlateBrush Brush;
    Brush.DrawAs = ESlateBrushDrawType::Box;
    Brush.TintColor = FSlateColor(Color);
    return Brush;
}

FSlateBrush MakeTextureBrush(UTexture2D* Texture)
{
    FSlateBrush Brush;
    Brush.DrawAs = ESlateBrushDrawType::Image;
    Brush.SetResourceObject(Texture);
    if (Texture)
    {
        Brush.ImageSize = FVector2D(
            Texture->GetSizeX(), Texture->GetSizeY());
    }
    return Brush;
}

void AddStatusTextUnique(TArray<FText>& StatusTexts, const FText& Text)
{
    if (!StatusTexts.ContainsByPredicate(
            [&Text](const FText& Existing)
            {
                return Existing.EqualTo(Text);
            }))
    {
        StatusTexts.Add(Text);
    }
}
}

UCMControlHUDWidget::UCMControlHUDWidget(
    const FObjectInitializer& ObjectInitializer
)
    : Super(ObjectInitializer)
{
    InputConfig = ENKMUIWidgetInputMode::GameAndMenu;
    GameMouseCaptureMode = EMouseCaptureMode::NoCapture;
    WireframeLabelFont = FCoreStyle::GetDefaultFontStyle(
        TEXT("Regular"), 14);
}

void UCMControlHUDWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    CachedHUDContainer = WidgetTree->FindWidget(TEXT("HUDContainer"));
    if (CachedHUDContainer)
    {
        CachedHUDContainer->SetVisibility(ESlateVisibility::Collapsed);
        CachedHUDContainer->RemoveFromParent();
        CachedHUDContainer = nullptr;
    }

    SetVisibility(ESlateVisibility::HitTestInvisible);
    RuntimeWireframeCameraRotation = WireframeCameraRotation;
    RuntimeWireframeZoom = 1.0f;
    InitializeWireframeHUD();
}

void UCMControlHUDWidget::NativeDestruct()
{
    TeardownWireframeHUD();
    Super::NativeDestruct();
}

void UCMControlHUDWidget::NativeTick(
    const FGeometry& MyGeometry,
    float InDeltaTime
)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    UpdateWireframeCameraInput();
    UpdateWireframePanelLayout(MyGeometry);
    RefreshWireframeCallouts(MyGeometry, InDeltaTime);
}

int32 UCMControlHUDWidget::NativePaint(
    const FPaintArgs& Args,
    const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId,
    const FWidgetStyle& InWidgetStyle,
    bool bParentEnabled
) const
{
    int32 CurrentLayer = Super::NativePaint(
        Args,
        AllottedGeometry,
        MyCullingRect,
        OutDrawElements,
        LayerId,
        InWidgetStyle,
        bParentEnabled);

    if (!WireframeRenderImage
        || WireframeRenderImage->GetVisibility() == ESlateVisibility::Collapsed)
    {
        return CurrentLayer;
    }

    const FSlateBrush* WhiteBrush =
        FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
    if (WireframeCaptureActor && WireframeCaptureActor->GetRenderTarget()
        && !WireframeImageSize.IsNearlyZero())
    {
        FSlateBrush CaptureBrush;
        CaptureBrush.DrawAs = ESlateBrushDrawType::Image;
        CaptureBrush.SetResourceObject(
            WireframeCaptureActor->GetRenderTarget());
        FSlateDrawElement::MakeBox(
            OutDrawElements,
            ++CurrentLayer,
            AllottedGeometry.ToPaintGeometry(
                WireframeImageSize,
                FSlateLayoutTransform(WireframeImageTopLeft)),
            &CaptureBrush,
            ESlateDrawEffect::InvertAlpha,
            FLinearColor::White);
    }

    const float WorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    const float CycleTime = FMath::Fmod(WorldTime, StatusCycleSeconds);
    const int32 CycleIndex = FMath::FloorToInt(
        WorldTime / StatusCycleSeconds);

    for (const FWireframeCallout& Callout : WireframeCallouts)
    {
        const FVector2D LabelEdge(
            Callout.bRightSide
                ? Callout.LabelPosition.X
                : Callout.LabelPosition.X + Callout.LabelSize.X,
            Callout.LabelPosition.Y + Callout.LabelSize.Y * 0.5f);
        const float ImageEdgeX = Callout.bRightSide
            ? WireframeImageTopLeft.X + WireframeImageSize.X + 5.0f
            : WireframeImageTopLeft.X - 5.0f;
        const FVector2D Elbow(ImageEdgeX, LabelEdge.Y);
        TArray<FVector2D> LinePoints = {
            Callout.AnchorPosition,
            Elbow,
            LabelEdge
        };
        FSlateDrawElement::MakeLines(
            OutDrawElements,
            ++CurrentLayer,
            AllottedGeometry.ToPaintGeometry(),
            LinePoints,
            ESlateDrawEffect::None,
            Callout.PlayerColor,
            true,
            1.25f);

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            ++CurrentLayer,
            AllottedGeometry.ToPaintGeometry(
                FVector2D(6.0f),
                FSlateLayoutTransform(
                    Callout.AnchorPosition - FVector2D(3.0f))),
            WhiteBrush,
            ESlateDrawEffect::None,
            Callout.PlayerColor);

        FText DisplayText = Callout.PlayerName;
        float TextOpacity = 1.0f;
        if (!Callout.StatusTexts.IsEmpty())
        {
            if (CycleTime >= StatusTextStart
                && CycleTime < StatusTextEnd)
            {
                DisplayText = Callout.StatusTexts[
                    CycleIndex % Callout.StatusTexts.Num()];
            }
            else if (CycleTime >= StatusNameEnd)
            {
                TextOpacity = 0.0f;
            }
        }
        if (TextOpacity <= 0.0f)
        {
            continue;
        }

        FSlateFontInfo Font = WireframeLabelFont;
        Font.Size = Callout.FontSize;
        FVector2D TextPosition = Callout.LabelPosition;
        if (!Callout.bRightSide && FSlateApplication::IsInitialized())
        {
            const FVector2D TextSize = FSlateApplication::Get()
                .GetRenderer()->GetFontMeasureService()
                ->Measure(DisplayText, Font);
            TextPosition.X += FMath::Max(
                Callout.LabelSize.X - TextSize.X,
                0.0f);
        }
        FSlateDrawElement::MakeText(
            OutDrawElements,
            ++CurrentLayer,
            AllottedGeometry.ToPaintGeometry(
                Callout.LabelSize,
                FSlateLayoutTransform(TextPosition)),
            DisplayText,
            Font,
            ESlateDrawEffect::None,
            Callout.PlayerColor.CopyWithNewOpacity(TextOpacity));
    }

    return CurrentLayer;
}

void UCMControlHUDWidget::SetControlBody(
    ACMControlBody* NewControlBody
)
{
    if (ControlBody.Get() == NewControlBody && SharedChimera.IsValid())
    {
        InitializeWireframeHUD();
        return;
    }

    TeardownWireframeHUD();
    ControlBody = NewControlBody;
    SharedChimera = NewControlBody
        ? NewControlBody->GetSharedChimera()
        : nullptr;
    InitializeWireframeHUD();
}

bool UCMControlHUDWidget::CacheWidgetTreeReferences()
{
    CachedHUDContainer = WidgetTree->FindWidget(TEXT("HUDContainer"));
    CachedStaminaProgressBar = Cast<UProgressBar>(
        WidgetTree->FindWidget(TEXT("StaminaProgressBar")));

    SegmentRows.Reset();
    PhysicalPartSlots.Reset();
    SegmentRows.Reserve(CMControl::MaxSegments);
    PhysicalPartSlots.Reserve(CMControl::MaxPartSlots);

    for (int32 SegmentIndex = 0;
        SegmentIndex < CMControl::MaxSegments;
        ++SegmentIndex)
    {
        FSegmentRowVisual& Row = SegmentRows.AddDefaulted_GetRef();
        Row.Root = WidgetTree->FindWidget(FName(*FString::Printf(
            TEXT("SegmentRow%d"), SegmentIndex)));
        Row.BodyBorder = Cast<UBorder>(WidgetTree->FindWidget(
            FName(*FString::Printf(TEXT("BodyBorder%d"), SegmentIndex))));
        Row.BodyHealthFill = Cast<UProgressBar>(WidgetTree->FindWidget(
            FName(*FString::Printf(TEXT("BodyHealthBar%d"), SegmentIndex))));
        Row.BodyControlText = Cast<UTextBlock>(WidgetTree->FindWidget(
            FName(*FString::Printf(TEXT("BodyControlText%d"), SegmentIndex))));
        Row.BodyStrikeLine = Cast<UBorder>(WidgetTree->FindWidget(
            FName(*FString::Printf(TEXT("BodyStrikeLine%d"), SegmentIndex))));
        if (!Row.Root || !Row.BodyBorder
            || !Row.BodyHealthFill || !Row.BodyControlText
            || !Row.BodyStrikeLine)
        {
            return false;
        }
        Row.BodyStrikeLine->SetVisibility(ESlateVisibility::Collapsed);
    }

    for (int32 FlatSlotIndex = 0;
        FlatSlotIndex < CMControl::MaxPartSlots;
        ++FlatSlotIndex)
    {
        FPartSlotVisual& Visual = PhysicalPartSlots.AddDefaulted_GetRef();
        Visual.Root = WidgetTree->FindWidget(FName(*FString::Printf(
            TEXT("PartSlotRoot%d"), FlatSlotIndex)));
        Visual.BaseImage = Cast<UImage>(WidgetTree->FindWidget(
            FName(*FString::Printf(TEXT("PartBaseImage%d"), FlatSlotIndex))));
        Visual.HealthFill = Cast<UProgressBar>(WidgetTree->FindWidget(
            FName(*FString::Printf(TEXT("PartHealthBar%d"), FlatSlotIndex))));
        Visual.KeyText = Cast<UTextBlock>(WidgetTree->FindWidget(
            FName(*FString::Printf(TEXT("PartKeyText%d"), FlatSlotIndex))));
        Visual.PartText = Cast<UTextBlock>(WidgetTree->FindWidget(
            FName(*FString::Printf(TEXT("PartLabelText%d"), FlatSlotIndex))));
        if (!Visual.Root || !Visual.BaseImage || !Visual.HealthFill
            || !Visual.KeyText || !Visual.PartText)
        {
            return false;
        }
    }

    return CachedHUDContainer && CachedStaminaProgressBar;
}

void UCMControlHUDWidget::BindStateDelegates()
{
    ACMControlBody* CurrentControlBody = ControlBody.Get();
    ACMChimera* CurrentChimera = SharedChimera.Get();
    if (!CurrentControlBody || !CurrentChimera)
    {
        return;
    }

    CurrentControlBody->OnControlSlotsChanged.AddUniqueDynamic(
        this, &ThisClass::HandleControlSlotsChanged);
    CurrentControlBody->OnControlInputChanged.AddUniqueDynamic(
        this, &ThisClass::HandleControlInputChanged);
    CurrentControlBody->OnPlayerStateChanged.AddUniqueDynamic(
        this, &ThisClass::HandleControlPlayerStateChanged);
    CurrentChimera->OnSegmentStatesChanged.AddUniqueDynamic(
        this, &ThisClass::HandleSegmentStatesChanged);

    RebindObservedPlayerState();

    if (UAbilitySystemComponent* AbilitySystem =
        CurrentChimera->GetAbilitySystemComponent())
    {
        StaminaChangedHandle = AbilitySystem
            ->GetGameplayAttributeValueChangeDelegate(
                UCMChimeraAttributeSet::GetStaminaAttribute())
            .AddUObject(this, &ThisClass::HandleStaminaChanged);
        MaxStaminaChangedHandle = AbilitySystem
            ->GetGameplayAttributeValueChangeDelegate(
                UCMChimeraAttributeSet::GetMaxStaminaAttribute())
            .AddUObject(this, &ThisClass::HandleStaminaChanged);
    }

    RebindObservedSlotsAndParts();
}

void UCMControlHUDWidget::UnbindStateDelegates()
{
    if (ACMControlBody* CurrentControlBody = ControlBody.Get())
    {
        CurrentControlBody->OnControlSlotsChanged.RemoveDynamic(
            this, &ThisClass::HandleControlSlotsChanged);
        CurrentControlBody->OnControlInputChanged.RemoveDynamic(
            this, &ThisClass::HandleControlInputChanged);
        CurrentControlBody->OnPlayerStateChanged.RemoveDynamic(
            this, &ThisClass::HandleControlPlayerStateChanged);
    }
    if (ACMChimera* CurrentChimera = SharedChimera.Get())
    {
        CurrentChimera->OnSegmentStatesChanged.RemoveDynamic(
            this, &ThisClass::HandleSegmentStatesChanged);
        if (UAbilitySystemComponent* AbilitySystem =
            CurrentChimera->GetAbilitySystemComponent())
        {
            AbilitySystem->GetGameplayAttributeValueChangeDelegate(
                UCMChimeraAttributeSet::GetStaminaAttribute())
                .Remove(StaminaChangedHandle);
            AbilitySystem->GetGameplayAttributeValueChangeDelegate(
                UCMChimeraAttributeSet::GetMaxStaminaAttribute())
                .Remove(MaxStaminaChangedHandle);
        }
    }
    if (ACMPlayerState* PlayerState = ObservedPlayerState.Get())
    {
        PlayerState->OnPlayerColorChanged.RemoveDynamic(
            this, &ThisClass::HandlePlayerColorChanged);
    }

    UnbindObservedSlotsAndParts();
    StaminaChangedHandle.Reset();
    MaxStaminaChangedHandle.Reset();
    ObservedPlayerState.Reset();
}

void UCMControlHUDWidget::RebindObservedPlayerState()
{
    ACMControlBody* CurrentControlBody = ControlBody.Get();
    ACMPlayerState* NewPlayerState = CurrentControlBody
        ? CurrentControlBody->GetPlayerState<ACMPlayerState>()
        : nullptr;
    if (ObservedPlayerState.Get() == NewPlayerState)
    {
        return;
    }

    if (ACMPlayerState* PreviousPlayerState = ObservedPlayerState.Get())
    {
        PreviousPlayerState->OnPlayerColorChanged.RemoveDynamic(
            this, &ThisClass::HandlePlayerColorChanged);
    }
    ObservedPlayerState = NewPlayerState;
    if (NewPlayerState)
    {
        NewPlayerState->OnPlayerColorChanged.AddUniqueDynamic(
            this, &ThisClass::HandlePlayerColorChanged);
    }
}

void UCMControlHUDWidget::RebindObservedSlotsAndParts()
{
    UnbindObservedSlotsAndParts();

    ACMControlBody* CurrentControlBody = ControlBody.Get();
    ACMChimera* CurrentChimera = SharedChimera.Get();
    if (!CurrentControlBody || !CurrentChimera)
    {
        return;
    }

    for (int32 ControlIndex = 0;
        ControlIndex < CMControl::MaxKeysPerPlayer;
        ++ControlIndex)
    {
        const FCMPartSlotAddress Address =
            CurrentControlBody
                ->GetEffectivePartSlotAddressForControlInput(ControlIndex);
        UCMPartSlotComponent* PartSlot =
            CurrentChimera->GetPartSlotComponent(Address);
        if (!PartSlot)
        {
            continue;
        }

        PartSlot->OnAttachedPartChanged.AddUniqueDynamic(
            this, &ThisClass::HandlePartAttachmentChanged);
        ObservedPartSlots.AddUnique(PartSlot);

        ACMPartActorBase* PartActor = Cast<ACMPartActorBase>(
            PartSlot->GetAttachedPart());
        if (PartActor && !ObservedParts.Contains(PartActor))
        {
            PartActor->OnHealthChanged.AddUniqueDynamic(
                this, &ThisClass::HandlePartHealthChanged);
            ObservedParts.Add(PartActor);
        }
    }
}

void UCMControlHUDWidget::UnbindObservedSlotsAndParts()
{
    for (const TWeakObjectPtr<UCMPartSlotComponent>& PartSlot
        : ObservedPartSlots)
    {
        if (PartSlot.IsValid())
        {
            PartSlot->OnAttachedPartChanged.RemoveDynamic(
                this, &ThisClass::HandlePartAttachmentChanged);
        }
    }
    for (const TWeakObjectPtr<ACMPartActorBase>& Part : ObservedParts)
    {
        if (Part.IsValid())
        {
            Part->OnHealthChanged.RemoveDynamic(
                this, &ThisClass::HandlePartHealthChanged);
        }
    }
    ObservedPartSlots.Reset();
    ObservedParts.Reset();
}

void UCMControlHUDWidget::RefreshAll()
{
    if (!CachedHUDContainer || !CachedStaminaProgressBar)
    {
        return;
    }

    const bool bHasState = ControlBody.IsValid() && SharedChimera.IsValid();
    CachedHUDContainer->SetVisibility(bHasState
        ? ESlateVisibility::HitTestInvisible
        : ESlateVisibility::Collapsed);
    if (!bHasState)
    {
        if (WireframeRenderImage)
        {
            WireframeRenderImage->SetVisibility(ESlateVisibility::Collapsed);
        }
        return;
    }

    if (WireframeRenderImage)
    {
        WireframeRenderImage->SetVisibility(
            ESlateVisibility::HitTestInvisible);
    }

    RefreshStamina();
    RefreshBodySegments();
    RefreshAssignedParts();
}

void UCMControlHUDWidget::RefreshStamina()
{
    const ACMChimera* CurrentChimera = SharedChimera.Get();
    if (CachedStaminaProgressBar && CurrentChimera)
    {
        CachedStaminaProgressBar->SetPercent(GetPercent(
            CurrentChimera->GetStamina(), CurrentChimera->GetMaxStamina()));
    }
}

void UCMControlHUDWidget::RefreshBodySegments()
{
    ACMControlBody* CurrentControlBody = ControlBody.Get();
    ACMChimera* CurrentChimera = SharedChimera.Get();
    if (!CurrentControlBody || !CurrentChimera)
    {
        return;
    }

    const int32 ActiveSegmentCount = FMath::Min(
        CurrentChimera->GetActiveSegmentCount(), SegmentRows.Num());
    const TArray<FCMBodySegmentHealthState> SegmentStates =
        CurrentChimera->GetSegmentHealthStates();
    const TArray<FCMPartSlotAddress>& ControlSlots =
        CurrentControlBody->GetControlSlots();
    static const TCHAR* ControlKeyNames[] = {
        TEXT("Q"), TEXT("W"), TEXT("E"), TEXT("R")
    };
    const ACMPlayerState* PlayerState = ObservedPlayerState.Get();
    const FLinearColor PlayerColor = PlayerState
        ? PlayerState->GetPlayerColor()
        : FLinearColor(0.95f, 0.8f, 0.15f, 1.0f);

    for (int32 SegmentIndex = 0;
        SegmentIndex < SegmentRows.Num();
        ++SegmentIndex)
    {
        FSegmentRowVisual& Row = SegmentRows[SegmentIndex];
        const bool bActive = SegmentIndex < ActiveSegmentCount;
        Row.Root->SetVisibility(bActive
            ? ESlateVisibility::HitTestInvisible
            : ESlateVisibility::Collapsed);
        if (!bActive)
        {
            continue;
        }

        FString OwnedControlKeys;
        for (int32 ControlIndex = 0;
            ControlIndex < ControlSlots.Num()
                && ControlIndex < CMControl::MaxKeysPerPlayer;
            ++ControlIndex)
        {
            if (ControlSlots[ControlIndex].SegmentIndex != SegmentIndex)
            {
                continue;
            }
            if (!OwnedControlKeys.IsEmpty())
            {
                OwnedControlKeys += TEXT("/");
            }
            OwnedControlKeys += ControlKeyNames[ControlIndex];
        }
        const bool bOwned = !OwnedControlKeys.IsEmpty();
        const FCMBodySegmentHealthState* SegmentState =
            SegmentStates.IsValidIndex(SegmentIndex)
                ? &SegmentStates[SegmentIndex]
                : nullptr;
        const bool bDead = SegmentState && SegmentState->bDead;
        Row.BodyBorder->SetBrushColor(bDead
            ? DeadSegmentColor
            : bOwned
                ? PlayerColor
                : FLinearColor(0.22f, 0.22f, 0.22f, 0.9f));
        Row.BodyHealthFill->SetPercent(bOwned && SegmentState
            ? GetPercent(SegmentState->Health, SegmentState->MaxHealth)
            : 0.0f);
        Row.BodyControlText->SetStrikeBrush(FSlateBrush());
        Row.BodyControlText->SetText(bOwned
            ? FText::FromString(OwnedControlKeys)
            : FText::GetEmpty());
        Row.BodyControlText->SetRenderOpacity(bDead ? 0.35f : 1.0f);
        Row.BodyStrikeLine->SetVisibility(bOwned && bDead
            ? ESlateVisibility::HitTestInvisible
            : ESlateVisibility::Collapsed);
    }
}

void UCMControlHUDWidget::RefreshAssignedParts()
{
    ACMControlBody* CurrentControlBody = ControlBody.Get();
    ACMChimera* CurrentChimera = SharedChimera.Get();
    if (!CurrentControlBody || !CurrentChimera)
    {
        return;
    }

    for (FPartSlotVisual& Visual : PhysicalPartSlots)
    {
        ResetPartSlotVisual(Visual);
    }

    static const TCHAR* KeyNames[] = {
        TEXT("Q"), TEXT("W"), TEXT("E"), TEXT("R")
    };
    const int32 ActiveSegmentCount =
        CurrentChimera->GetActiveSegmentCount();
    for (int32 ControlIndex = 0;
        ControlIndex < CMControl::MaxKeysPerPlayer;
        ++ControlIndex)
    {
        const FCMPartSlotAddress Address =
            CurrentControlBody
                ->GetEffectivePartSlotAddressForControlInput(ControlIndex);
        if (!CMControl::IsValidPartSlot(Address, ActiveSegmentCount))
        {
            continue;
        }

        const int32 FlatSlotIndex = CMControl::ToFlatPartSlotIndex(Address);
        if (!PhysicalPartSlots.IsValidIndex(FlatSlotIndex))
        {
            continue;
        }

        FPartSlotVisual& Visual = PhysicalPartSlots[FlatSlotIndex];
        Visual.Root->SetVisibility(ESlateVisibility::HitTestInvisible);
        Visual.KeyText->SetText(FText::FromString(KeyNames[ControlIndex]));
        Visual.KeyText->SetColorAndOpacity(
            FSlateColor(CurrentControlBody->IsControlSlotEnabled(ControlIndex)
                ? IdleControlKeyColor
                : DisabledControlKeyColor));

        const UCMPartSlotComponent* PartSlot =
            CurrentChimera->GetPartSlotComponent(Address);
        const ACMPartActorBase* PartActor = PartSlot
            ? Cast<ACMPartActorBase>(PartSlot->GetAttachedPart())
            : nullptr;
        UTexture2D* PartTexture = GetPartTexture(PartActor);
        const FVector2D PartImageScale = CMControl::IsRightPartSlot(Address)
            ? FVector2D(-1.0f, 1.0f)
            : FVector2D(1.0f, 1.0f);
        Visual.BaseImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
        Visual.BaseImage->SetRenderScale(PartImageScale);
        Visual.HealthFill->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
        Visual.HealthFill->SetRenderScale(PartImageScale);
        Visual.BaseImage->SetBrushFromTexture(PartTexture, true);
        Visual.BaseImage->SetVisibility(PartTexture
            ? ESlateVisibility::HitTestInvisible
            : ESlateVisibility::Collapsed);
        FProgressBarStyle HealthStyle = Visual.HealthFill->GetWidgetStyle();
        HealthStyle.SetBackgroundImage(MakeSolidBrush(FLinearColor::Transparent));
        HealthStyle.SetFillImage(MakeTextureBrush(PartTexture));
        Visual.HealthFill->SetWidgetStyle(HealthStyle);
        Visual.HealthFill->SetPercent(PartActor
            ? GetPercent(PartActor->GetHealth(), PartActor->GetMaxHealth())
            : 0.0f);
        Visual.HealthFill->SetVisibility(PartTexture
            ? ESlateVisibility::HitTestInvisible
            : ESlateVisibility::Collapsed);
        Visual.PartText->SetText(GetPartLabel(PartActor));
        Visual.Root->SetRenderOpacity(1.0f);
    }
}

void UCMControlHUDWidget::ResetPartSlotVisual(
    FPartSlotVisual& Visual
)
{
    Visual.Root->SetVisibility(ESlateVisibility::Hidden);
    Visual.Root->SetRenderTranslation(FVector2D::ZeroVector);
    Visual.Root->SetRenderOpacity(1.0f);
}

void UCMControlHUDWidget::RefreshPartHealth()
{
    ACMControlBody* CurrentControlBody = ControlBody.Get();
    ACMChimera* CurrentChimera = SharedChimera.Get();
    if (!CurrentControlBody || !CurrentChimera)
    {
        return;
    }

    for (int32 ControlIndex = 0;
        ControlIndex < CMControl::MaxKeysPerPlayer;
        ++ControlIndex)
    {
        const FCMPartSlotAddress Address =
            CurrentControlBody
                ->GetEffectivePartSlotAddressForControlInput(ControlIndex);
        if (!CMControl::IsValidPartSlot(
                Address, CurrentChimera->GetActiveSegmentCount()))
        {
            continue;
        }

        const int32 FlatSlotIndex = CMControl::ToFlatPartSlotIndex(Address);
        if (!PhysicalPartSlots.IsValidIndex(FlatSlotIndex))
        {
            continue;
        }

        const UCMPartSlotComponent* PartSlot =
            CurrentChimera->GetPartSlotComponent(Address);
        const ACMPartActorBase* PartActor = PartSlot
            ? Cast<ACMPartActorBase>(PartSlot->GetAttachedPart())
            : nullptr;
        PhysicalPartSlots[FlatSlotIndex].HealthFill->SetPercent(PartActor
            ? GetPercent(PartActor->GetHealth(), PartActor->GetMaxHealth())
            : 0.0f);
    }
}

void UCMControlHUDWidget::SetControlSlotHighlighted(
    int32 ControlIndex,
    bool bPressed
)
{
    ACMControlBody* CurrentControlBody = ControlBody.Get();
    ACMChimera* CurrentChimera = SharedChimera.Get();
    if (!CurrentControlBody || !CurrentChimera)
    {
        return;
    }

    const FCMPartSlotAddress Address =
        CurrentControlBody
            ->GetEffectivePartSlotAddressForControlInput(ControlIndex);
    if (!CMControl::IsValidPartSlot(
            Address, CurrentChimera->GetActiveSegmentCount()))
    {
        return;
    }
    const int32 FlatSlotIndex = CMControl::ToFlatPartSlotIndex(Address);
    if (!PhysicalPartSlots.IsValidIndex(FlatSlotIndex))
    {
        return;
    }

    PhysicalPartSlots[FlatSlotIndex].Root->SetRenderTranslation(bPressed
        ? FVector2D(0.0f, -7.0f)
        : FVector2D::ZeroVector);
    PhysicalPartSlots[FlatSlotIndex].Root->SetRenderOpacity(1.0f);
    PhysicalPartSlots[FlatSlotIndex].KeyText->SetColorAndOpacity(
        FSlateColor(bPressed
            ? FLinearColor::White
            : IdleControlKeyColor));
}

void UCMControlHUDWidget::HandleControlSlotsChanged()
{
    RebindObservedPlayerState();
    RebindObservedSlotsAndParts();
    RefreshBodySegments();
    RefreshAssignedParts();
}

void UCMControlHUDWidget::HandleControlInputChanged(
    int32 SlotIndex,
    bool bPressed
)
{
    SetControlSlotHighlighted(SlotIndex, bPressed);
}

void UCMControlHUDWidget::HandleControlPlayerStateChanged()
{
    RebindObservedPlayerState();
    RefreshBodySegments();
}

void UCMControlHUDWidget::HandleSegmentStatesChanged()
{
    RefreshBodySegments();
    RefreshAssignedParts();
}

void UCMControlHUDWidget::HandlePartAttachmentChanged(
    UCMPartSlotComponent* PartSlot,
    AActor* AttachedPart
)
{
    RebindObservedSlotsAndParts();
    RefreshAssignedParts();
}

void UCMControlHUDWidget::HandlePartHealthChanged(
    float PreviousHealth,
    float CurrentHealth,
    float MaxHealth
)
{
    RefreshPartHealth();
}

void UCMControlHUDWidget::InitializeWireframeHUD()
{
    if (!WireframeRenderImage)
    {
        WireframeRenderImage = Cast<UImage>(
            WidgetTree->FindWidget(TEXT("WireframeRenderImage")));
    }
    if (!WireframeRenderImage)
    {
        WireframeCanvas = Cast<UCanvasPanel>(
            WidgetTree->FindWidget(TEXT("WireframeCanvas")));
        if (!WireframeCanvas)
        {
            WireframeCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
        }
        if (!WireframeCanvas)
        {
            UOverlay* RootOverlay = Cast<UOverlay>(
                WidgetTree->FindWidget(TEXT("RootCanvas")));
            if (!RootOverlay)
            {
                RootOverlay = Cast<UOverlay>(WidgetTree->RootWidget);
            }
            if (RootOverlay)
            {
                WireframeCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
                    UCanvasPanel::StaticClass(),
                    TEXT("WireframeCanvas"));
                UOverlaySlot* OverlaySlot =
                    RootOverlay->AddChildToOverlay(WireframeCanvas);
                OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
                OverlaySlot->SetVerticalAlignment(VAlign_Fill);
            }
        }
        if (!WireframeCanvas)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("Wireframe HUD requires a CanvasPanel root, an Overlay root, or a named WireframeRenderImage."));
            return;
        }

        WireframeRenderImage = WidgetTree->ConstructWidget<UImage>(
            UImage::StaticClass(),
            TEXT("WireframeRenderImage"));
        UCanvasPanelSlot* CanvasSlot =
            WireframeCanvas->AddChildToCanvas(WireframeRenderImage);
        CanvasSlot->SetAnchors(FAnchors(
            WireframePanelAnchor.X,
            WireframePanelAnchor.Y));
        CanvasSlot->SetAlignment(WireframePanelAlignment);
        CanvasSlot->SetPosition(WireframePanelOffset);
        CanvasSlot->SetSize(FVector2D(340.0f));
        CanvasSlot->SetZOrder(20);
        WireframeRenderImage->SetOpacity(0.96f);
        WireframeRenderImage->SetVisibility(ESlateVisibility::Collapsed);
    }

    // The UImage is only a Canvas layout anchor. Its default brush resolves to
    // Slate's white texture, so letting it paint creates an opaque white quad
    // underneath the separately composited scene-capture brush.
    FSlateBrush LayoutOnlyBrush;
    LayoutOnlyBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
    WireframeRenderImage->SetBrush(LayoutOnlyBrush);

    ACMChimera* CurrentChimera = SharedChimera.Get();
    if (!CurrentChimera || WireframeCaptureActor)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.ObjectFlags |= RF_Transient;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    WireframeCaptureActor = World->SpawnActor<
        ACMWireframeHUDCaptureActor>(SpawnParameters);
    if (!WireframeCaptureActor)
    {
        UE_LOG(LogTemp, Error,
            TEXT("Failed to spawn the local Wireframe HUD capture actor."));
        return;
    }

    WireframeCaptureActor->Initialize(
        CurrentChimera,
        GetOrCreateWireframeHealthCurve(),
        WireframeCameraRotation);
    WireframeRenderImage->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UCMControlHUDWidget::TeardownWireframeHUD()
{
    WireframeCallouts.Reset();
    SmoothedCalloutPositions.Reset();
    CalloutRightSideById.Reset();
    bWireframeOrbitActive = false;
    if (WireframeRenderImage)
    {
        WireframeRenderImage->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (WireframeCaptureActor)
    {
        WireframeCaptureActor->Destroy();
        WireframeCaptureActor = nullptr;
    }
}

void UCMControlHUDWidget::UpdateWireframePanelLayout(
    const FGeometry& MyGeometry
)
{
    if (!WireframeRenderImage)
    {
        return;
    }

    UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(
        WireframeRenderImage->Slot);
    if (!CanvasSlot)
    {
        return;
    }

    const FVector2D ViewSize = MyGeometry.GetLocalSize();
    const float LabelWidth = FMath::Clamp(
        ViewSize.X * 0.115f, 104.0f, 148.0f);
    const float DesiredImageSize = FMath::Min(
        ViewSize.Y * 0.42f,
        ViewSize.X - LabelWidth * 2.0f - 56.0f);
    const float ImageSize = FMath::Clamp(
        DesiredImageSize, 180.0f, 380.0f);
    CanvasSlot->SetAnchors(FAnchors(
        WireframePanelAnchor.X,
        WireframePanelAnchor.Y));
    CanvasSlot->SetAlignment(WireframePanelAlignment);
    CanvasSlot->SetPosition(WireframePanelOffset);
    CanvasSlot->SetSize(FVector2D(ImageSize));

    const FGeometry ImageGeometry = WireframeRenderImage->GetCachedGeometry();
    WireframeImageTopLeft = MyGeometry.AbsoluteToLocal(
        ImageGeometry.LocalToAbsolute(FVector2D::ZeroVector));
    WireframeImageSize = ImageGeometry.GetLocalSize();
}

void UCMControlHUDWidget::UpdateWireframeCameraInput()
{
    APlayerController* OwningPlayer = GetOwningPlayer();
    if (!OwningPlayer || !WireframeCaptureActor || !WireframeRenderImage)
    {
        return;
    }

    const FGeometry ImageGeometry = WireframeRenderImage->GetCachedGeometry();
    const bool bCursorOverPanel = FSlateApplication::IsInitialized()
        && ImageGeometry.IsUnderLocation(
            FSlateApplication::Get().GetCursorPos());

    if (OwningPlayer->WasInputKeyJustPressed(EKeys::RightMouseButton)
        && bCursorOverPanel)
    {
        bWireframeOrbitActive = true;
    }
    if (!OwningPlayer->IsInputKeyDown(EKeys::RightMouseButton))
    {
        bWireframeOrbitActive = false;
    }

    if (bWireframeOrbitActive)
    {
        float MouseDeltaX = 0.0f;
        float MouseDeltaY = 0.0f;
        OwningPlayer->GetInputMouseDelta(MouseDeltaX, MouseDeltaY);
        RuntimeWireframeCameraRotation.Yaw +=
            MouseDeltaX * WireframeOrbitSensitivity;
        RuntimeWireframeCameraRotation.Pitch = FMath::Clamp(
            RuntimeWireframeCameraRotation.Pitch
                - MouseDeltaY * WireframeOrbitSensitivity,
            FMath::Min(WireframePitchLimits.X, WireframePitchLimits.Y),
            FMath::Max(WireframePitchLimits.X, WireframePitchLimits.Y));
        RuntimeWireframeCameraRotation.Normalize();
    }

    if (bCursorOverPanel)
    {
        const float MinZoom = FMath::Max(
            FMath::Min(WireframeZoomLimits.X, WireframeZoomLimits.Y),
            0.01f);
        const float MaxZoom = FMath::Max(
            FMath::Max(WireframeZoomLimits.X, WireframeZoomLimits.Y),
            MinZoom);
        if (OwningPlayer->WasInputKeyJustPressed(EKeys::MouseScrollUp))
        {
            RuntimeWireframeZoom = FMath::Clamp(
                RuntimeWireframeZoom - WireframeZoomStep,
                MinZoom,
                MaxZoom);
        }
        else if (OwningPlayer->WasInputKeyJustPressed(
                     EKeys::MouseScrollDown))
        {
            RuntimeWireframeZoom = FMath::Clamp(
                RuntimeWireframeZoom + WireframeZoomStep,
                MinZoom,
                MaxZoom);
        }
    }

    WireframeCaptureActor->SetCameraView(
        RuntimeWireframeCameraRotation,
        RuntimeWireframeZoom);
}

void UCMControlHUDWidget::RefreshWireframeCallouts(
    const FGeometry& MyGeometry,
    float InDeltaTime
)
{
    ACMChimera* CurrentChimera = SharedChimera.Get();
    if (!CurrentChimera || !WireframeCaptureActor
        || WireframeImageSize.IsNearlyZero())
    {
        WireframeCallouts.Reset();
        return;
    }

    const APlayerController* OwningPlayer = GetOwningPlayer();
    if (!OwningPlayer)
    {
        WireframeCallouts.Reset();
        return;
    }
    const bool bShowPlayerLabels =
        OwningPlayer->IsInputKeyDown(EKeys::Tab);

    TMap<FCMPartSlotAddress, FText> LocalKeyBySlot;
    ACMControlBody* LocalControlBody = ControlBody.Get();
    ACMPlayerState* LocalPlayerState = LocalControlBody
        ? LocalControlBody->GetPlayerState<ACMPlayerState>()
        : nullptr;
    if (!bShowPlayerLabels && LocalControlBody)
    {
        static const TCHAR* ControlKeyNames[] = {
            TEXT("Q"), TEXT("W"), TEXT("E"), TEXT("R")
        };
        for (int32 ControlIndex = 0;
            ControlIndex < CMControl::MaxKeysPerPlayer;
            ++ControlIndex)
        {
            if (!LocalControlBody->IsControlSlotEnabled(ControlIndex))
            {
                continue;
            }
            const FCMPartSlotAddress Address = LocalControlBody
                ->GetEffectivePartSlotAddressForControlInput(ControlIndex);
            if (CMControl::IsValidPartSlot(
                    Address, CurrentChimera->GetActiveSegmentCount()))
            {
                LocalKeyBySlot.FindOrAdd(Address) =
                    FText::FromString(ControlKeyNames[ControlIndex]);
            }
        }
    }

    TMap<FCMPartSlotAddress, ACMPlayerState*> OwnerBySlot;
    TMap<ACMPlayerState*, TArray<FText>> StatusesByOwner;
    for (TActorIterator<ACMControlBody> It(GetWorld()); It; ++It)
    {
        ACMControlBody* Body = *It;
        ACMPlayerState* PlayerState = Body
            ? Body->GetPlayerState<ACMPlayerState>()
            : nullptr;
        if (!Body || !PlayerState
            || Body->GetSharedChimera() != CurrentChimera)
        {
            continue;
        }

        for (const FCMPartSlotAddress& Address : Body->GetControlSlots())
        {
            OwnerBySlot.FindOrAdd(Address) = PlayerState;
        }
        if (Body->IsConfused())
        {
            AddStatusTextUnique(
                StatusesByOwner.FindOrAdd(PlayerState),
                LOCTEXT("WireStatusConfused", "혼란"));
        }
        if (Body->IsDelirious())
        {
            AddStatusTextUnique(
                StatusesByOwner.FindOrAdd(PlayerState),
                LOCTEXT("WireStatusDelirious", "착란"));
        }
    }

    const int32 ActiveSegmentCount = CurrentChimera->GetActiveSegmentCount();
    for (int32 FlatSlotIndex = 0;
        FlatSlotIndex < ActiveSegmentCount * CMControl::PartSlotsPerSegment;
        ++FlatSlotIndex)
    {
        const FCMPartSlotAddress Address =
            CMControl::FromFlatPartSlotIndex(FlatSlotIndex);
        UCMPartSlotComponent* PartSlot =
            CurrentChimera->GetPartSlotComponent(Address);
        ACMPartActorBase* Part = PartSlot
            ? Cast<ACMPartActorBase>(PartSlot->GetAttachedPart())
            : nullptr;
        ACMPlayerState* const* Owner = OwnerBySlot.Find(Address);
        const UCMPartStatusComponent* Status = Part
            ? Part->GetPartStatusComponent()
            : nullptr;
        if (!Owner || !*Owner || !Part || !Status)
        {
            continue;
        }

        TArray<FText>& StatusTexts = StatusesByOwner.FindOrAdd(*Owner);
        const FGameplayTagContainer& Tags = Status->GetActiveStatusTags();
        if (Tags.HasTagExact(CMPartStatusTags::Electrified))
        {
            AddStatusTextUnique(
                StatusTexts,
                LOCTEXT("WireStatusParalyzed", "마비"));
        }
        if (Tags.HasTagExact(CMPartStatusTags::Slowed))
        {
            AddStatusTextUnique(
                StatusTexts,
                LOCTEXT("WireStatusSlowed", "둔화"));
        }
    }

    TArray<FWireframeCallout> NewCallouts;
    auto AddCallout = [this, &NewCallouts, &StatusesByOwner](
        const FName StableId,
        const FVector& WorldAnchor,
        const FText& LabelText,
        const FLinearColor& LabelColor,
        ACMPlayerState* StatusOwner)
    {
        if (LabelText.IsEmpty())
        {
            return;
        }
        FVector2D NormalizedAnchor;
        if (!WireframeCaptureActor->ProjectWorldLocation(
                WorldAnchor, NormalizedAnchor))
        {
            return;
        }

        FWireframeCallout& Callout = NewCallouts.AddDefaulted_GetRef();
        Callout.StableId = StableId;
        const FVector2D ClampedAnchor(
            FMath::Clamp(NormalizedAnchor.X, 0.01f, 0.99f),
            FMath::Clamp(NormalizedAnchor.Y, 0.01f, 0.99f));
        Callout.AnchorPosition =
            WireframeImageTopLeft + ClampedAnchor * WireframeImageSize;
        Callout.PlayerName = LabelText;
        if (StatusOwner)
        {
            if (const TArray<FText>* StatusTexts =
                StatusesByOwner.Find(StatusOwner))
            {
                Callout.StatusTexts = *StatusTexts;
            }
        }
        Callout.PlayerColor = LabelColor;

        bool& bRight = CalloutRightSideById.FindOrAdd(
            StableId,
            NormalizedAnchor.X >= 0.5f);
        if (bRight && NormalizedAnchor.X < 0.42f)
        {
            bRight = false;
        }
        else if (!bRight && NormalizedAnchor.X > 0.58f)
        {
            bRight = true;
        }
        Callout.bRightSide = bRight;
    };

    for (int32 FlatSlotIndex = 0;
        FlatSlotIndex < ActiveSegmentCount * CMControl::PartSlotsPerSegment;
        ++FlatSlotIndex)
    {
        const FCMPartSlotAddress Address =
            CMControl::FromFlatPartSlotIndex(FlatSlotIndex);
        UCMPartSlotComponent* PartSlot =
            CurrentChimera->GetPartSlotComponent(Address);
        if (!PartSlot)
        {
            continue;
        }

        const FName StableId(*FString::Printf(
            TEXT("Slot.%d.%d"),
            Address.SegmentIndex,
            Address.PartSlotIndex));
        if (bShowPlayerLabels)
        {
            ACMPlayerState* Owner = OwnerBySlot.FindRef(Address);
            if (Owner)
            {
                AddCallout(
                    StableId,
                    PartSlot->GetComponentLocation(),
                    FText::FromString(Owner->GetPlayerName()),
                    Owner->GetPlayerColor(),
                    Owner);
            }
        }
        else if (const FText* ControlKey = LocalKeyBySlot.Find(Address))
        {
            AddCallout(
                StableId,
                PartSlot->GetComponentLocation(),
                *ControlKey,
                LocalPlayerState
                    ? LocalPlayerState->GetPlayerColor()
                    : FLinearColor::White,
                nullptr);
        }
    }

    TArray<int32> LeftIndices;
    TArray<int32> RightIndices;
    for (int32 Index = 0; Index < NewCallouts.Num(); ++Index)
    {
        (NewCallouts[Index].bRightSide ? RightIndices : LeftIndices).Add(Index);
    }
    auto SortByAnchorY = [&NewCallouts](const int32 A, const int32 B)
    {
        return NewCallouts[A].AnchorPosition.Y
            < NewCallouts[B].AnchorPosition.Y;
    };
    LeftIndices.Sort(SortByAnchorY);
    RightIndices.Sort(SortByAnchorY);

    const FVector2D ViewSize = MyGeometry.GetLocalSize();
    const float LabelWidth = FMath::Clamp(
        ViewSize.X * 0.115f, 104.0f, 148.0f);
    auto LayoutRail = [this, &NewCallouts, LabelWidth, ViewSize, InDeltaTime](
        const TArray<int32>& Indices,
        bool bRight)
    {
        if (Indices.IsEmpty())
        {
            return;
        }
        const float Step = WireframeImageSize.Y
            / static_cast<float>(Indices.Num());
        const float LabelHeight = FMath::Clamp(Step - 2.0f, 13.0f, 24.0f);
        const int32 FontSize = FMath::Clamp(
            FMath::FloorToInt(LabelHeight - 6.0f), 10, 14);
        const float LabelX = bRight
            ? WireframeImageTopLeft.X + WireframeImageSize.X + 10.0f
            : WireframeImageTopLeft.X - LabelWidth - 10.0f;

        for (int32 Order = 0; Order < Indices.Num(); ++Order)
        {
            FWireframeCallout& Callout = NewCallouts[Indices[Order]];
            Callout.LabelSize = FVector2D(LabelWidth, LabelHeight);
            Callout.FontSize = FontSize;
            const FVector2D TargetPosition(
                FMath::Clamp(LabelX, 4.0f, ViewSize.X - LabelWidth - 4.0f),
                FMath::Clamp(
                    WireframeImageTopLeft.Y + Step * (Order + 0.5f)
                        - LabelHeight * 0.5f,
                    4.0f,
                    ViewSize.Y - LabelHeight - 4.0f));
            FVector2D& SmoothedPosition =
                SmoothedCalloutPositions.FindOrAdd(
                    Callout.StableId,
                    TargetPosition);
            SmoothedPosition = FMath::Vector2DInterpTo(
                SmoothedPosition,
                TargetPosition,
                InDeltaTime,
                12.0f);
            Callout.LabelPosition = SmoothedPosition;
        }
    };
    LayoutRail(LeftIndices, false);
    LayoutRail(RightIndices, true);
    WireframeCallouts = MoveTemp(NewCallouts);
}

UCurveLinearColor*
UCMControlHUDWidget::GetOrCreateWireframeHealthCurve()
{
    if (WireframeHealthColorCurve)
    {
        return WireframeHealthColorCurve;
    }
    if (TransientHealthColorCurve)
    {
        return TransientHealthColorCurve;
    }

    TransientHealthColorCurve = NewObject<UCurveLinearColor>(
        this,
        TEXT("DefaultWireframeHealthColorCurve"));
    const FLinearColor Keys[] = {
        FLinearColor(0.18f, 0.005f, 0.005f, 1.0f),
        FLinearColor(1.00f, 0.025f, 0.010f, 1.0f),
        FLinearColor(0.025f, 1.00f, 0.060f, 1.0f)
    };
    const float Times[] = {0.0f, 0.5f, 1.0f};
    for (int32 Channel = 0; Channel < 4; ++Channel)
    {
        for (int32 KeyIndex = 0; KeyIndex < UE_ARRAY_COUNT(Keys); ++KeyIndex)
        {
            const float Value = Channel == 0 ? Keys[KeyIndex].R
                : Channel == 1 ? Keys[KeyIndex].G
                : Channel == 2 ? Keys[KeyIndex].B
                : Keys[KeyIndex].A;
            const FKeyHandle Handle =
                TransientHealthColorCurve->FloatCurves[Channel]
                    .UpdateOrAddKey(Times[KeyIndex], Value);
            TransientHealthColorCurve->FloatCurves[Channel]
                .SetKeyInterpMode(Handle, RCIM_Linear);
        }
    }
    return TransientHealthColorCurve;
}

void UCMControlHUDWidget::HandlePlayerColorChanged()
{
    RefreshBodySegments();
}

void UCMControlHUDWidget::HandleStaminaChanged(
    const FOnAttributeChangeData& ChangeData
)
{
    RefreshStamina();
}

UTexture2D* UCMControlHUDWidget::GetPartTexture(
    const ACMPartActorBase* PartActor
) const
{
    if (!PartActor)
    {
        return nullptr;
    }

    switch (ICMPartInterface::Execute_GetPartType(
        const_cast<ACMPartActorBase*>(PartActor)))
    {
    case ECMPartSlotType::Head:
        return HeadPartTexture;
    case ECMPartSlotType::Arm:
        return ArmPartTexture;
    case ECMPartSlotType::Leg:
        return LegPartTexture;
    default:
        return nullptr;
    }
}

FText UCMControlHUDWidget::GetPartLabel(
    const ACMPartActorBase* PartActor
) const
{
    return PartActor
        ? FText::Format(
            LOCTEXT("PartTier", "T{0}"),
            FText::AsNumber(PartActor->GetTierLevel()))
        : FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
