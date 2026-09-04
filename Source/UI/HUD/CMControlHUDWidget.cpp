#include "HUD/CMControlHUDWidget.h"

#include "Ability/CMChimeraAttributeSet.h"
#include "Parts/Core/CMPartActorBase.h"
#include "Player/CMChimera.h"
#include "Player/CMControlBody.h"
#include "Player/CMPartInterface.h"
#include "Player/CMPartSlotComponent.h"
#include "Player/CMPlayerState.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"

#define LOCTEXT_NAMESPACE "CMControlHUDWidget"

namespace
{
const FLinearColor IdleControlKeyColor(0.55f, 0.55f, 0.55f, 0.65f);
const FLinearColor DisabledControlKeyColor(0.22f, 0.22f, 0.22f, 0.45f);
const FLinearColor DeadSegmentColor(0.10f, 0.10f, 0.10f, 0.95f);

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
}

UCMControlHUDWidget::UCMControlHUDWidget(
    const FObjectInitializer& ObjectInitializer
)
    : Super(ObjectInitializer)
{
    InputConfig = ENKMUIWidgetInputMode::GameAndMenu;
    GameMouseCaptureMode = EMouseCaptureMode::NoCapture;
}

void UCMControlHUDWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (!HeadPartTexture)
    {
        HeadPartTexture = LoadObject<UTexture2D>(nullptr,
            TEXT("/Game/Chimera/UI/HUD/T_UI_Part_Head.T_UI_Part_Head"));
    }
    if (!ArmPartTexture)
    {
        ArmPartTexture = LoadObject<UTexture2D>(nullptr,
            TEXT("/Game/Chimera/UI/HUD/T_UI_Part_Arm.T_UI_Part_Arm"));
    }
    if (!LegPartTexture)
    {
        LegPartTexture = LoadObject<UTexture2D>(nullptr,
            TEXT("/Game/Chimera/UI/HUD/T_UI_Part_Leg.T_UI_Part_Leg"));
    }
    if (!CacheWidgetTreeReferences())
    {
        UE_LOG(LogTemp, Error,
            TEXT("WBP_CMControlHUD is missing required named widgets."));
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    SetVisibility(ESlateVisibility::HitTestInvisible);
    CachedHUDContainer->SetVisibility(ESlateVisibility::Collapsed);
    RefreshAll();
}

void UCMControlHUDWidget::NativeDestruct()
{
    UnbindStateDelegates();
    Super::NativeDestruct();
}

void UCMControlHUDWidget::SetControlBody(
    ACMControlBody* NewControlBody
)
{
    if (ControlBody.Get() == NewControlBody && SharedChimera.IsValid())
    {
        RebindObservedPlayerState();
        RefreshAll();
        return;
    }

    UnbindStateDelegates();
    ControlBody = NewControlBody;
    SharedChimera = NewControlBody
        ? NewControlBody->GetSharedChimera()
        : nullptr;
    BindStateDelegates();
    RefreshAll();
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
            CurrentControlBody->GetPartSlotAddressForControlSlot(ControlIndex);
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
        return;
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
    const int32 OwnedSegmentIndex =
        CurrentControlBody->GetOwnedSegmentIndex();
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

        const bool bFirstOwned = SegmentIndex == OwnedSegmentIndex;
        const bool bSecondOwned = SegmentIndex == OwnedSegmentIndex + 1;
        const bool bOwned = bFirstOwned || bSecondOwned;
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
        Row.BodyControlText->SetText(bFirstOwned
            ? LOCTEXT("QWBody", "Q/W")
            : bSecondOwned
                ? LOCTEXT("ERBody", "E/R")
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
            CurrentControlBody->GetPartSlotAddressForControlSlot(ControlIndex);
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
            CurrentControlBody->GetPartSlotAddressForControlSlot(ControlIndex);
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
        CurrentControlBody->GetPartSlotAddressForControlSlot(ControlIndex);
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
