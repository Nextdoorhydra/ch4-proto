#include "Stage/Trigger/CMBasicButtonBase.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Parts/Arm/CMArmPart.h"
#include "Stage/CMStageCommandTags.h"
#include "Stage/Trigger/Component/CMActivationTriggerComponent.h"
#include "TimerManager.h"

ACMBasicButtonBase::ACMBasicButtonBase()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    bExposeDirectCommandTags = false;

    HitVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("HitVolume"));
    HitVolume->SetupAttachment(SceneRoot);
    HitVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    HitVolume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    HitVolume->SetGenerateOverlapEvents(true);

    ButtonVisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ButtonVisualRoot"));
    ButtonVisualRoot->SetupAttachment(SceneRoot);
    ButtonVisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ButtonVisualMesh"));
    ButtonVisualMesh->SetupAttachment(ButtonVisualRoot);
    ButtonVisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 일반 버튼은 대상의 초기 상태와 관계없이 현재 활성 상태를 반전
    TargetCommandTag = CMStageCommandTags::Mechanism_Toggle;
    ReleaseCommandTag = CMStageCommandTags::Mechanism_Toggle;
}

// 일반 버튼의 토글 및 일회성 정책 초기화
void ACMBasicButtonBase::BeginPlay()
{
    // 기존 BP나 레벨 인스턴스에 저장된 명시적 태그와 관계없이 일반 버튼은 토글로 통일
    TargetCommandTag = CMStageCommandTags::Mechanism_Toggle;
    ReleaseCommandTag = CMStageCommandTags::Mechanism_Toggle;
    if (bToggleOnHit)
    {
        ActivationTrigger->bOneShot = false;
    }
    Super::BeginPlay();

    ReleasedVisualLocation = ButtonVisualRoot->GetRelativeLocation();
    const FVector PressDirection = LocalPressDirection.GetSafeNormal(
        SMALL_NUMBER, FVector(0.0f, 0.0f, -1.0f));
    PressedVisualLocation = ReleasedVisualLocation
        + PressDirection * FMath::Max(PressDepth, 0.0f);
    ButtonMaterial = ButtonVisualMesh->CreateDynamicMaterialInstance(
        FMath::Max(MaterialSlotIndex, 0));

    OnPresentationStateChanged.AddUniqueDynamic(
        this, &ThisClass::HandlePresentationStateChanged);
    HandlePresentationStateChanged(GetPresentationState());
    ApplyVisualState();
}

void ACMBasicButtonBase::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(PulseReturnTimerHandle);
    OnPresentationStateChanged.RemoveDynamic(
        this, &ThisClass::HandlePresentationStateChanged);
    Super::EndPlay(EndPlayReason);
}

void ACMBasicButtonBase::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    const float Duration = TargetVisualAlpha > VisualAlpha
        ? FMath::Max(PressDuration, 0.0f)
        : FMath::Max(ReleaseDuration, 0.0f);
    VisualAlpha = Duration > SMALL_NUMBER
        ? FMath::FInterpConstantTo(
            VisualAlpha, TargetVisualAlpha, DeltaSeconds, 1.0f / Duration)
        : TargetVisualAlpha;
    if (FMath::IsNearlyEqual(VisualAlpha, TargetVisualAlpha))
    {
        VisualAlpha = TargetVisualAlpha;
    }

    ApplyVisualState();
    SetActorTickEnabled(!FMath::IsNearlyEqual(
        VisualAlpha, TargetVisualAlpha));
}

void ACMBasicButtonBase::HandleElementReset_Implementation()
{
    Super::HandleElementReset_Implementation();
    GetWorldTimerManager().ClearTimer(PulseReturnTimerHandle);
    SetVisualTarget(false);
}

void ACMBasicButtonBase::NotifySwingHit(
    ACMArmPart* ArmPart, UPrimitiveComponent* HitComponent)
{
    if (!HasAuthority() || !IsValid(ArmPart) || !ArmPart->HasAuthority()
        || !ArmPart->IsSwinging() || !ArmPart->IsOperational()
        || HitComponent != HitVolume || !IsElementActive())
    {
        return;
    }

    const FGuid AttackId = ArmPart->GetCurrentSwingAttackId();
    if (!AttackId.IsValid())
    {
        return;
    }
    for (auto It = LastSwingAttackIds.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid())
        {
            It.RemoveCurrent();
        }
    }
    FGuid& LastAttackId = LastSwingAttackIds.FindOrAdd(ArmPart);
    if (LastAttackId == AttackId)
    {
        return;
    }
    LastAttackId = AttackId;
    HandleValidButtonInput(ArmPart);
}

ECMStageTriggerSignal ACMBasicButtonBase::ResolveTriggerSignal(
    bool bActivated) const
{
    // 반복 토글 버튼은 현재 눌림 상태를 퍼즐 조건에 전달하고 일회성 버튼은 순간 입력만 전달
    if (bToggleOnHit)
    {
        return bActivated
            ? ECMStageTriggerSignal::Activated
            : ECMStageTriggerSignal::Deactivated;
    }
    return ECMStageTriggerSignal::Pulse;
}

// 반복 버튼은 눌림과 해제를 교대하고 일회성 버튼은 최초 눌림만 전달
void ACMBasicButtonBase::HandleValidButtonInput(AActor* TriggeringActor)
{
    if (bToggleOnHit && ActivationTrigger->IsTriggered())
    {
        ReleaseButton(TriggeringActor);
    }
    else if (PressButton(TriggeringActor) && !bToggleOnHit)
    {
        MulticastPlayPulseFeedback();
    }
}

void ACMBasicButtonBase::HandlePresentationStateChanged(
    const FCMTriggerPresentationState& State)
{
    if (!State.bReady)
    {
        return;
    }

    bPresentationEnabled = State.bEnabled;
    if (!bPresentationEnabled)
    {
        GetWorldTimerManager().ClearTimer(PulseReturnTimerHandle);
        SetVisualTarget(false);
    }
    else if (bToggleOnHit)
    {
        SetVisualTarget(State.bTriggered);
    }
    else if (!State.bTriggered)
    {
        GetWorldTimerManager().ClearTimer(PulseReturnTimerHandle);
        SetVisualTarget(false);
    }
    else
    {
        ApplyVisualState();
    }
}

void ACMBasicButtonBase::MulticastPlayPulseFeedback_Implementation()
{
    if (bToggleOnHit || !bPresentationEnabled)
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(PulseReturnTimerHandle);
    SetVisualTarget(true);
    const float ReturnDelay = FMath::Max(PressDuration, 0.0f)
        + FMath::Max(PulseHoldDuration, 0.0f);
    if (ReturnDelay <= SMALL_NUMBER)
    {
        ReturnPulseVisual();
        return;
    }
    GetWorldTimerManager().SetTimer(
        PulseReturnTimerHandle,
        this,
        &ThisClass::ReturnPulseVisual,
        ReturnDelay,
        false);
}

void ACMBasicButtonBase::SetVisualTarget(bool bPressed)
{
    TargetVisualAlpha = bPressed ? 1.0f : 0.0f;
    if ((bPressed ? PressDuration : ReleaseDuration) <= SMALL_NUMBER)
    {
        VisualAlpha = TargetVisualAlpha;
        ApplyVisualState();
        return;
    }
    SetActorTickEnabled(true);
}

void ACMBasicButtonBase::ReturnPulseVisual()
{
    SetVisualTarget(false);
}

void ACMBasicButtonBase::ApplyVisualState()
{
    if (ButtonVisualRoot)
    {
        ButtonVisualRoot->SetRelativeLocation(FMath::Lerp(
            ReleasedVisualLocation, PressedVisualLocation, VisualAlpha));
    }
    if (!ButtonMaterial)
    {
        return;
    }

    const FLinearColor Color = bPresentationEnabled
        ? FMath::Lerp(OffColor, OnColor, VisualAlpha)
        : DisabledColor;
    const float EmissiveIntensity = bPresentationEnabled
        ? FMath::Lerp(
            FMath::Max(OffEmissiveIntensity, 0.0f),
            FMath::Max(OnEmissiveIntensity, 0.0f),
            VisualAlpha)
        : 0.0f;
    if (!ColorParameterName.IsNone())
    {
        ButtonMaterial->SetVectorParameterValue(ColorParameterName, Color);
    }
    if (!EmissiveParameterName.IsNone())
    {
        ButtonMaterial->SetScalarParameterValue(
            EmissiveParameterName, EmissiveIntensity);
    }
}
