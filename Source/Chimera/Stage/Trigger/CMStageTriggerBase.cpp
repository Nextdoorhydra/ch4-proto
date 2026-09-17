#include "Stage/Trigger/CMStageTriggerBase.h"
#include "Net/UnrealNetwork.h"

#include "Components/SceneComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Stage/CMStageCommandTags.h"
#include "Stage/CMStageElementComponent.h"
#include "Stage/Trigger/Component/CMActivationTriggerComponent.h"

ACMStageTriggerBase::ACMStageTriggerBase()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    ActivationTrigger = CreateDefaultSubobject<UCMActivationTriggerComponent>(TEXT("ActivationTrigger"));
    TargetCommandTag = CMStageCommandTags::Mechanism_Activate;
    ReleaseCommandTag = CMStageCommandTags::Mechanism_Deactivate;
}

// 런타임에 공통 트리거 상태를 StageDirector 대상 명령에 연결
void ACMStageTriggerBase::BeginPlay()
{
    ActivationTrigger->OnStateUpdated.AddUObject(this, &ThisClass::RefreshPresentationState);
    Super::BeginPlay();
    ActivationTrigger->OnActivated.AddUniqueDynamic(this, &ThisClass::HandleTriggerActivated);
    ActivationTrigger->OnDeactivated.AddUniqueDynamic(this, &ThisClass::HandleTriggerDeactivated);
    RefreshPresentationState();
    ResolveBillboardIndicator();
    if (HasBillboardIndicator())
    {
        SetActorTickEnabled(true);
        UpdateBillboardIndicator();
    }
}

void ACMStageTriggerBase::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateBillboardIndicator();
}

void ACMStageTriggerBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ActivationTrigger->OnStateUpdated.RemoveAll(this);
    ActivationTrigger->OnActivated.RemoveDynamic(this, &ThisClass::HandleTriggerActivated);
    ActivationTrigger->OnDeactivated.RemoveDynamic(this, &ThisClass::HandleTriggerDeactivated);
    Super::EndPlay(EndPlayReason);
}

void ACMStageTriggerBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ThisClass, PresentationState);
}

void ACMStageTriggerBase::FillPresentationState(FCMTriggerPresentationState& State) const
{
    State.bReady = true;
    State.bEnabled = ActivationTrigger->IsTriggerEnabled();
    State.bTriggered = ActivationTrigger->IsTriggered();
    State.bCanActivate = ActivationTrigger->CanActivate();
}

void ACMStageTriggerBase::RefreshPresentationState()
{
    if (!HasAuthority()) return;
    FCMTriggerPresentationState NewState;
    FillPresentationState(NewState);
    if (NewState == PresentationState) return;
    PresentationState = NewState;
    ForceNetUpdate();
    OnRep_PresentationState();
}

void ACMStageTriggerBase::OnRep_PresentationState()
{
    // Never replay gameplay signals from replicated UI data.
    OnPresentationStateChanged.Broadcast(PresentationState);
}

// 하위 구현에서 검증한 작동 조건을 공통 트리거에 전달
bool ACMStageTriggerBase::ActivateTrigger(AActor* TriggeringActor)
{
    return HasAuthority() && ActivationTrigger->TryActivate(TriggeringActor);
}

// 하위 구현에서 검증한 해제 조건을 공통 트리거에 전달
bool ACMStageTriggerBase::DeactivateTrigger(AActor* TriggeringActor)
{
    return HasAuthority()
        && ActivationTrigger->SetTriggeredState(false, TriggeringActor);
}

void ACMStageTriggerBase::SetDirectTargetCommandEnabled(bool bEnabled)
{
    if (HasAuthority())
    {
        bDirectTargetCommandEnabled = bEnabled;
    }
}

// 장치 활성 상태를 실제 트리거 입력 허용 상태에 연결
void ACMStageTriggerBase::HandleElementActiveChanged_Implementation(bool bIsActive)
{
    Super::HandleElementActiveChanged_Implementation(bIsActive);
    if (HasAuthority())
    {
        ActivationTrigger->SetTriggerEnabled(bIsActive);
    }
}

// 트리거 작동 이력을 레벨 시작 상태로 복원
void ACMStageTriggerBase::HandleElementReset_Implementation()
{
    Super::HandleElementReset_Implementation();
    ActivationTrigger->ResetTrigger();
}

// 활성 조건이 충족되면 설정된 대상 명령과 표현 이벤트를 실행
void ACMStageTriggerBase::HandleTriggerActivated(AActor* TriggeringActor)
{
    if (bDirectTargetCommandEnabled)
    {
        StageElement->RequestStageCommand(
            ResolveTargetPlacementId(),
            TargetGroup,
            TargetCommandTag);
    }
    OnTriggerSignal.Broadcast(this, ResolveTriggerSignal(true));
    OnTriggerActivated(TriggeringActor);
}

// 활성 조건이 해제되면 설정된 해제 명령과 표현 이벤트를 실행
void ACMStageTriggerBase::HandleTriggerDeactivated(AActor* TriggeringActor)
{
    if (bDirectTargetCommandEnabled)
    {
        StageElement->RequestStageCommand(
            ResolveTargetPlacementId(),
            TargetGroup,
            ReleaseCommandTag);
    }
    OnTriggerSignal.Broadcast(this, ResolveTriggerSignal(false));
    OnTriggerDeactivated(TriggeringActor);
}

ECMStageTriggerSignal ACMStageTriggerBase::ResolveTriggerSignal(
    bool bActivated) const
{
    return bActivated
        ? ECMStageTriggerSignal::Activated
        : ECMStageTriggerSignal::Deactivated;
}

// 직접 선택한 액터의 StageElement ID를 우선 사용하고 수동 ID를 대체 경로로 사용
FName ACMStageTriggerBase::ResolveTargetPlacementId() const
{
    if (IsValid(TargetActor))
    {
        if (const UCMStageElementComponent* TargetStageElement =
                TargetActor->FindComponentByClass<UCMStageElementComponent>())
        {
            return TargetStageElement->PlacementId;
        }
    }
    return TargetPlacementId;
}

bool ACMStageTriggerBase::HasBillboardIndicator() const
{
    return IsValid(BillboardIndicator)
        && GetNetMode() != NM_DedicatedServer;
}

void ACMStageTriggerBase::ResolveBillboardIndicator()
{
    static const TArray<FName> IndicatorNames = {
        TEXT("CMPowerSourceIndicator"),
        TEXT("CMPressurePlateIndicator"),
        TEXT("CMVisionStoneIndicator")
    };

    TArray<USceneComponent*> SceneComponents;
    GetComponents(SceneComponents);
    for (USceneComponent* SceneComponent : SceneComponents)
    {
        if (IsValid(SceneComponent)
            && IndicatorNames.Contains(SceneComponent->GetFName()))
        {
            BillboardIndicator = SceneComponent;
            return;
        }
    }
}

void ACMStageTriggerBase::UpdateBillboardIndicator()
{
    if (!HasBillboardIndicator())
    {
        return;
    }

    const APlayerController* PlayerController =
        GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    const APlayerCameraManager* CameraManager = PlayerController
        ? PlayerController->PlayerCameraManager
        : nullptr;
    if (!CameraManager)
    {
        return;
    }

    const FVector IndicatorLocation = BillboardIndicator->GetComponentLocation();
    const FVector CameraLocation = CameraManager->GetCameraLocation();
    if (BillboardMaxDistance > 0.0f
        && FVector::DistSquared(IndicatorLocation, CameraLocation)
            > FMath::Square(BillboardMaxDistance))
    {
        return;
    }

    BillboardIndicator->SetWorldRotation(
        (CameraLocation - IndicatorLocation).Rotation());
}
