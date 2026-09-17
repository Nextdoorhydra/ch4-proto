#include "Stage/CMStageElementBase.h"

#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"
#include "Stage/CMStageCommandTags.h"
#include "Stage/CMStageElementComponent.h"
#include "Stage/Trigger/Component/CMPowerSocketComponent.h"

ACMStageElementBase::ACMStageElementBase()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    StageElement = CreateDefaultSubobject<UCMStageElementComponent>(TEXT("StageElement"));
}

// StageDirector 명령을 구독하고 서버의 초기 활성 상태를 계산
void ACMStageElementBase::BeginPlay()
{
    Super::BeginPlay();
    StageElement->OnStageCommandReceived.AddUniqueDynamic(
        this, &ThisClass::HandleStageCommand);

    if (!PowerSocket)
    {
        PowerSocket = FindComponentByClass<UCMPowerSocketComponent>();
    }
    if (PowerSocket)
    {
        PowerSocket->OnPowerStateChanged.AddUniqueDynamic(
            this, &ThisClass::HandlePowerStateChanged);
    }

    if (HasAuthority())
    {
        bActivationRequested = bStartActive;
        const bool bPreviousActive = bElementActive;
        RefreshElementActiveState();
        if (bPreviousActive == bElementActive)
        {
            HandleElementActiveChanged(bElementActive);
        }
    }
    else
    {
        HandleElementActiveChanged(bElementActive);
    }
}

bool ACMStageElementBase::CanActivateElement() const
{
    return !PowerSocket || PowerSocket->IsPowered();
}

// 실제 활성 상태를 모든 클라이언트에 복제
void ACMStageElementBase::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ThisClass, bElementActive);
}

// 서버의 활성화 요청을 하위 요소 조건과 함께 다시 평가
void ACMStageElementBase::ActivateElement()
{
    if (HasAuthority())
    {
        bActivationRequested = true;
        RefreshElementActiveState();
        HandleElementActivationRequested();
    }
}

// 서버의 활성화 요청을 해제하고 실제 상태를 즉시 갱신
void ACMStageElementBase::DeactivateElement()
{
    if (HasAuthority())
    {
        bActivationRequested = false;
        RefreshElementActiveState();
    }
}

// 현재 요청 상태를 서버에서 반전하고 하위 활성 조건과 함께 다시 계산
void ACMStageElementBase::ToggleElement()
{
    if (HasAuthority())
    {
        bActivationRequested = !bActivationRequested;
        RefreshElementActiveState();
    }
}

// 실행 중 상태를 먼저 끈 뒤 하위 초기화와 시작 상태 복원을 순서대로 수행
void ACMStageElementBase::ResetElement()
{
    if (!HasAuthority())
    {
        return;
    }

    bActivationRequested = bStartActive;
    SetElementActive(false);
    HandleElementReset();
    RefreshElementActiveState();
}

// 요청 상태와 하위 요소의 준비 조건을 실제 활성 상태로 변환
void ACMStageElementBase::RefreshElementActiveState()
{
    if (HasAuthority())
    {
        SetElementActive(bActivationRequested && CanActivateElement());
    }
}

// 장애물과 장치가 공통으로 사용하는 활성화, 비활성화, 초기화 태그 처리
void ACMStageElementBase::HandleStageCommand(
    FGameplayTag CommandTag,
    UObject* CommandInstigator)
{
    if (!HasAuthority())
    {
        return;
    }

    if (CommandTag.MatchesTagExact(CMStageCommandTags::Mechanism_Activate)
        || CommandTag.MatchesTagExact(CMStageCommandTags::Effect_Activate))
    {
        ActivateElement();
    }
    else if (CommandTag.MatchesTagExact(CMStageCommandTags::Mechanism_Deactivate)
        || CommandTag.MatchesTagExact(CMStageCommandTags::Effect_Deactivate))
    {
        DeactivateElement();
    }
    else if (CommandTag.MatchesTagExact(CMStageCommandTags::Mechanism_Toggle))
    {
        ToggleElement();
    }
    else if (CommandTag.MatchesTagExact(CMStageCommandTags::Mechanism_Reset)
        || CommandTag.MatchesTagExact(CMStageCommandTags::Effect_Restart))
    {
        ResetElement();
    }
}

void ACMStageElementBase::HandlePowerStateChanged(bool bPowered)
{
    if (HasAuthority())
    {
        RefreshElementActiveState();
    }
}

// 서버에서 복제된 활성 상태를 클라이언트 표현에 반영
void ACMStageElementBase::OnRep_ElementActive()
{
    HandleElementActiveChanged(bElementActive);
}

// 실제 값이 달라진 경우에만 표현과 네트워크 갱신 수행
void ACMStageElementBase::SetElementActive(bool bNewActive)
{
    if (bElementActive == bNewActive)
    {
        return;
    }

    bElementActive = bNewActive;
    HandleElementActiveChanged(bElementActive);
    ForceNetUpdate();
}

void ACMStageElementBase::HandleElementActiveChanged_Implementation(bool bIsActive)
{
}

void ACMStageElementBase::HandleElementReset_Implementation()
{
}
