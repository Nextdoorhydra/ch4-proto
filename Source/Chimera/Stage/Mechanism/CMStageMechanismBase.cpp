#include "Stage/Mechanism/CMStageMechanismBase.h"

#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"
#include "Stage/CMStageCommandTags.h"
#include "Stage/CMStageElementComponent.h"

ACMStageMechanismBase::ACMStageMechanismBase()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    StageElement = CreateDefaultSubobject<UCMStageElementComponent>(TEXT("StageElement"));
}

// StageDirector 명령 수신과 초기 활성 상태를 준비
void ACMStageMechanismBase::BeginPlay()
{
    Super::BeginPlay();
    StageElement->OnStageCommandReceived.AddDynamic(this, &ThisClass::HandleStageCommand);
    bMechanismActive = bStartActive;
    HandleMechanismActiveChanged(bMechanismActive);
}

// 장치 활성 상태를 네트워크로 복제
void ACMStageMechanismBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ThisClass, bMechanismActive);
}

// 장치를 활성 상태로 전환
void ACMStageMechanismBase::ActivateMechanism()
{
    if (HasAuthority())
    {
        SetMechanismActive(true);
    }
}

// 장치를 비활성 상태로 전환
void ACMStageMechanismBase::DeactivateMechanism()
{
    if (HasAuthority())
    {
        SetMechanismActive(false);
    }
}

// 장치를 초기 활성 상태로 복원하고 하위 구현을 초기화
void ACMStageMechanismBase::ResetMechanism()
{
    if (!HasAuthority())
    {
        return;
    }

    SetMechanismActive(bStartActive);
    HandleMechanismReset();
}

// StageDirector의 Mechanism 명령을 공통 장치 동작에 연결
void ACMStageMechanismBase::HandleStageCommand(FGameplayTag CommandTag, UObject* CommandInstigator)
{
    if (!HasAuthority())
    {
        return;
    }

    if (CommandTag.MatchesTagExact(CMStageCommandTags::Mechanism_Activate))
    {
        ActivateMechanism();
    }
    else if (CommandTag.MatchesTagExact(CMStageCommandTags::Mechanism_Deactivate))
    {
        DeactivateMechanism();
    }
    else if (CommandTag.MatchesTagExact(CMStageCommandTags::Mechanism_Reset))
    {
        ResetMechanism();
    }
}

// 복제된 활성 상태를 클라이언트 장치 표현에 반영
void ACMStageMechanismBase::OnRep_MechanismActive()
{
    HandleMechanismActiveChanged(bMechanismActive);
}

// 값이 실제로 바뀐 경우에만 장치 표현을 갱신
void ACMStageMechanismBase::SetMechanismActive(bool bNewActive)
{
    if (bMechanismActive == bNewActive)
    {
        return;
    }

    bMechanismActive = bNewActive;
    HandleMechanismActiveChanged(bMechanismActive);
    ForceNetUpdate();
}

void ACMStageMechanismBase::HandleMechanismActiveChanged_Implementation(bool bIsActive)
{
}

void ACMStageMechanismBase::HandleMechanismReset_Implementation()
{
}
