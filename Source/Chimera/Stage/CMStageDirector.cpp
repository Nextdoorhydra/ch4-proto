#include "Stage/CMStageDirector.h"

#include "GameMode/Play/CMPlayGameMode.h"
#include "GameMode/Play/CMPlayGameState.h"
#include "Stage/CMStageEventMessage.h"
#include "Stage/CMStageEventTags.h"
#include "Stage/CMStageElementComponent.h"
#include "Stage/CMStageSequenceComponent.h"
#include "AsyncLoad/CMStageLoadCoordinatorSubsystem.h"
#include "AsyncLoad/CMStageLoadLog.h"
#include "Components/ActorComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Net/UnrealNetwork.h"

namespace
{
AActor* ResolveCommandInstigatorActor(UObject* CommandInstigator)
{
    if (AActor* Actor = Cast<AActor>(CommandInstigator))
    {
        return Actor;
    }
    if (const UActorComponent* Component = Cast<UActorComponent>(CommandInstigator))
    {
        return Component->GetOwner();
    }
    return nullptr;
}
}

// Tick 없이 서버 사건으로만 동작하도록 기본 Actor 설정
ACMStageDirector::ACMStageDirector()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bAlwaysRelevant = true;
    SequenceComponent = CreateDefaultSubobject<UCMStageSequenceComponent>(TEXT("StageSequence"));
}

void ACMStageDirector::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ThisClass, StageInstanceId);
}

// 플레이 맵 시작 시 서버 GameMode에 현재 StageDirector 등록
void ACMStageDirector::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        StageInstanceId = FGuid::NewGuid();
        if (ACMPlayGameMode* PlayGameMode = GetPlayGameMode())
        {
            PlayGameMode->RegisterStageDirector(this);
        }
    }
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UCMStageLoadCoordinatorSubsystem* Coordinator =
            GameInstance->GetSubsystem<UCMStageLoadCoordinatorSubsystem>())
        {
            Coordinator->OnLoadGroupFinished.AddUniqueDynamic(
                this, &ThisClass::HandleLoadGroupFinished);
        }
    }
    TryBindPlayGameState();
}

// 권한 서버에서 유효한 이벤트만 StageInstanceId와 함께 메시지 라우터로 전송
void ACMStageDirector::BroadcastStageEvent(FGameplayTag EventTag, UObject* EventInstigator)
{
    if (!HasAuthority() || !EventTag.IsValid() || !StageInstanceId.IsValid())
    {
        return;
    }

    FCMStageEventMessage Message;
    Message.StageInstanceId = StageInstanceId;
    Message.EventTag = EventTag;
    Message.Instigator = EventInstigator;
    UGameplayMessageSubsystem::Get(this).BroadcastMessage(
        CMStageEventTags::Message_Stage_Event,
        Message);
}

bool ACMStageDirector::RegisterStageElement(UCMStageElementComponent* Element)
{
    if (!IsValid(Element) || Element->PlacementId.IsNone())
    {
        UE_LOG(LogChimeraStageLoad, Error, TEXT("Stage element has no PlacementId. Actor=%s"),
            *GetNameSafe(Element ? Element->GetOwner() : nullptr));
        return false;
    }
    if (const TWeakObjectPtr<UCMStageElementComponent>* Existing = ElementsByPlacementId.Find(Element->PlacementId))
    {
        if (Existing->IsValid() && Existing->Get() != Element)
        {
            UE_LOG(LogChimeraStageLoad, Error, TEXT("Duplicate PlacementId. Id=%s Existing=%s New=%s"),
                *Element->PlacementId.ToString(), *GetNameSafe((*Existing)->GetOwner()), *GetNameSafe(Element->GetOwner()));
            return false;
        }
    }
    ElementsByPlacementId.Add(Element->PlacementId, Element);
    RegisteredElements.AddUnique(Element);
    return true;
}

void ACMStageDirector::UnregisterStageElement(UCMStageElementComponent* Element)
{
    if (!Element)
    {
        return;
    }
    if (ElementsByPlacementId.FindRef(Element->PlacementId).Get() == Element)
    {
        ElementsByPlacementId.Remove(Element->PlacementId);
    }
    RegisteredElements.Remove(Element);
}

void ACMStageDirector::ExecuteStageCommand(
    FName TargetPlacementId,
    FGameplayTag TargetGroup,
    FGameplayTag CommandTag,
    UObject* CommandInstigator)
{
    if (HasAuthority() && CommandTag.IsValid())
    {
        MulticastExecuteStageCommand(
            TargetPlacementId,
            TargetGroup,
            CommandTag,
            ResolveCommandInstigatorActor(CommandInstigator));
    }
}

void ACMStageDirector::MulticastExecuteStageCommand_Implementation(
    FName TargetPlacementId,
    FGameplayTag TargetGroup,
    FGameplayTag CommandTag,
    AActor* CommandInstigator)
{
    ExecuteStageCommandLocally(
        TargetPlacementId, TargetGroup, CommandTag, CommandInstigator);
}

void ACMStageDirector::ExecuteStageCommandLocally(
    FName TargetPlacementId,
    FGameplayTag TargetGroup,
    FGameplayTag CommandTag,
    UObject* CommandInstigator)
{
    TArray<UCMStageElementComponent*> Targets;
    if (!TargetPlacementId.IsNone())
    {
        if (UCMStageElementComponent* Element = ElementsByPlacementId.FindRef(TargetPlacementId).Get())
        {
            Targets.Add(Element);
        }
    }
    else if (TargetGroup.IsValid())
    {
        for (const TWeakObjectPtr<UCMStageElementComponent>& Element : RegisteredElements)
        {
            if (Element.IsValid() && Element->GroupTags.HasTagExact(TargetGroup))
            {
                Targets.Add(Element.Get());
            }
        }
    }

    if (Targets.IsEmpty())
    {
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("Stage command target was not found. Placement=%s Group=%s Command=%s"),
            *TargetPlacementId.ToString(), *TargetGroup.ToString(), *CommandTag.ToString());
        return;
    }
    for (UCMStageElementComponent* Target : Targets)
    {
        if (Target->CanExecuteOnCurrentMachine())
        {
            Target->ExecuteStageCommand(CommandTag, CommandInstigator);
        }
    }
}

void ACMStageDirector::ExecuteStageCommandAfterLoad(
    FName LoadGroupId,
    FName TargetPlacementId,
    FGameplayTag TargetGroup,
    FGameplayTag CommandTag,
    UObject* CommandInstigator)
{
    if (HasAuthority() && !LoadGroupId.IsNone() && CommandTag.IsValid())
    {
        MulticastExecuteStageCommandAfterLoad(
            LoadGroupId,
            TargetPlacementId,
            TargetGroup,
            CommandTag,
            ResolveCommandInstigatorActor(CommandInstigator));
    }
}

void ACMStageDirector::MulticastExecuteStageCommandAfterLoad_Implementation(
    FName LoadGroupId,
    FName TargetPlacementId,
    FGameplayTag TargetGroup,
    FGameplayTag CommandTag,
    AActor* CommandInstigator)
{
    UGameInstance* GameInstance = GetGameInstance();
    UCMStageLoadCoordinatorSubsystem* Coordinator = GameInstance
        ? GameInstance->GetSubsystem<UCMStageLoadCoordinatorSubsystem>() : nullptr;
    if (!Coordinator)
    {
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("Stage command cannot wait for load group because Coordinator is missing. Group=%s"),
            *LoadGroupId.ToString());
        return;
    }
    if (Coordinator->IsLoadGroupReady(LoadGroupId))
    {
        ExecuteStageCommandLocally(
            TargetPlacementId, TargetGroup, CommandTag, CommandInstigator);
        return;
    }

    FCMLocalPendingStageCommand& Pending = PendingCommandsByLoadGroup.FindOrAdd(LoadGroupId).AddDefaulted_GetRef();
    Pending.TargetPlacementId = TargetPlacementId;
    Pending.TargetGroup = TargetGroup;
    Pending.CommandTag = CommandTag;
    Pending.CommandInstigator = CommandInstigator;

    const ECMStageLoadGroupState State = Coordinator->GetLoadGroupState(LoadGroupId);
    if (State != ECMStageLoadGroupState::Loading
        && State != ECMStageLoadGroupState::PendingRelease
        && !Coordinator->RequestLoadGroup(LoadGroupId).IsValid())
    {
        PendingCommandsByLoadGroup.Remove(LoadGroupId);
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("Stage command references an unavailable LoadGroup. Group=%s Command=%s"),
            *LoadGroupId.ToString(), *CommandTag.ToString());
    }
}

void ACMStageDirector::HandleLoadGroupFinished(
    FName LoadGroupId,
    EAsyncLoadResult Result,
    bool bReleasedImmediately)
{
    TArray<FCMLocalPendingStageCommand> PendingCommands;
    if (!PendingCommandsByLoadGroup.RemoveAndCopyValue(LoadGroupId, PendingCommands))
    {
        return;
    }
    if (Result != EAsyncLoadResult::Succeeded || bReleasedImmediately)
    {
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("Pending stage commands were cancelled because LoadGroup failed or was released. Group=%s Count=%d"),
            *LoadGroupId.ToString(), PendingCommands.Num());
        return;
    }
    for (const FCMLocalPendingStageCommand& Pending : PendingCommands)
    {
        ExecuteStageCommandLocally(
            Pending.TargetPlacementId,
            Pending.TargetGroup,
            Pending.CommandTag,
            Pending.CommandInstigator.Get());
    }
}

void ACMStageDirector::RequestLoadGroup(FName LoadGroupId)
{
    if (HasAuthority() && !LoadGroupId.IsNone())
    {
        MulticastRequestLoadGroup(LoadGroupId);
    }
}

void ACMStageDirector::MulticastRequestLoadGroup_Implementation(FName LoadGroupId)
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UCMStageLoadCoordinatorSubsystem* Coordinator =
            GameInstance->GetSubsystem<UCMStageLoadCoordinatorSubsystem>())
        {
            Coordinator->RequestLoadGroup(LoadGroupId);
        }
    }
}

// 월드 종료 시 GameState 연출 델리게이트 연결 해제
void ACMStageDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (BoundPlayGameState)
    {
        BoundPlayGameState->OnStagePresentationChanged.RemoveAll(this);
    }
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UCMStageLoadCoordinatorSubsystem* Coordinator =
            GameInstance->GetSubsystem<UCMStageLoadCoordinatorSubsystem>())
        {
            Coordinator->OnLoadGroupFinished.RemoveAll(this);
        }
    }
    PendingCommandsByLoadGroup.Reset();
    Super::EndPlay(EndPlayReason);
}

// 현재 월드 GameState의 연출 상태를 로컬 Director에 연결
void ACMStageDirector::TryBindPlayGameState()
{
    ACMPlayGameState* PlayGameState = GetWorld()
        ? GetWorld()->GetGameState<ACMPlayGameState>() : nullptr;
    if (!PlayGameState || BoundPlayGameState == PlayGameState)
    {
        return;
    }
    BoundPlayGameState = PlayGameState;
    PlayGameState->OnStagePresentationChanged.AddUniqueDynamic(
        this, &ThisClass::HandleStagePresentationChanged);
    HandleStagePresentationChanged();
}

// 복제 상태에 따라 각 머신에서 시작 또는 결과 연출 실행
void ACMStageDirector::HandleStagePresentationChanged()
{
    if (!BoundPlayGameState)
    {
        return;
    }
    switch (BoundPlayGameState->GetStagePresentationState())
    {
    case ECMStagePresentationState::Starting:
        BeginStartingPresentation();
        break;
    case ECMStagePresentationState::Result:
        BeginResultPresentation();
        break;
    default:
        break;
    }
}

// 별도 연출 구현이 없으면 즉시 시작 완료 처리
void ACMStageDirector::BeginStartingPresentation_Implementation()
{
    FinishStartingPresentation();
}

// 중복 완료를 막고 Starting에서 Playing 전환 요청
void ACMStageDirector::FinishStartingPresentation()
{
    if (!HasAuthority() || bStartingPresentationFinished)
    {
        return;
    }

    bStartingPresentationFinished = true;
    if (ACMPlayGameMode* PlayGameMode = GetPlayGameMode())
    {
        PlayGameMode->HandleStartingPresentationFinished(this);
    }
}

// 별도 결과 연출 구현이 없으면 즉시 다음 스테이지 전환 허용
void ACMStageDirector::BeginResultPresentation_Implementation()
{
    FinishResultPresentation();
}

// 중복 보고를 막고 결과 연출 종료를 서버 GameMode에 전달
void ACMStageDirector::FinishResultPresentation()
{
    if (!HasAuthority() || bResultPresentationFinished)
    {
        return;
    }

    bResultPresentationFinished = true;
    if (ACMPlayGameMode* PlayGameMode = GetPlayGameMode())
    {
        PlayGameMode->HandleResultPresentationFinished(this);
    }
}

// 중복 결과를 막고 현재 스테이지 클리어 처리 요청
void ACMStageDirector::CompleteStage()
{
    if (!HasAuthority() || bStageResolved)
    {
        return;
    }

    bStageResolved = true;
    if (ACMPlayGameMode* PlayGameMode = GetPlayGameMode())
    {
        PlayGameMode->HandleStageCompleted(this);
    }
}

// 중복 결과를 막고 현재 스테이지 실패 처리 요청
void ACMStageDirector::FailStage()
{
    if (!HasAuthority() || bStageResolved)
    {
        return;
    }

    bStageResolved = true;
    if (ACMPlayGameMode* PlayGameMode = GetPlayGameMode())
    {
        PlayGameMode->HandleStageFailed(this);
    }
}

// 현재 월드의 서버 PlayGameMode 조회
ACMPlayGameMode* ACMStageDirector::GetPlayGameMode() const
{
    return GetWorld()
        ? GetWorld()->GetAuthGameMode<ACMPlayGameMode>()
        : nullptr;
}
