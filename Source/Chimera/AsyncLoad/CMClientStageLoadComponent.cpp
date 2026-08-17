#include "AsyncLoad/CMClientStageLoadComponent.h"

#include "AsyncLoad/CMStageLoadCoordinatorSubsystem.h"
#include "GameMode/Play/CMPlayGameState.h"
#include "Player/CMPlayerController.h"

UCMClientStageLoadComponent::UCMClientStageLoadComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

// 로컬 컨트롤러가 GameState 복제 요청을 받을 준비 시작
void UCMClientStageLoadComponent::BeginPlay()
{
    Super::BeginPlay();
    TryBindPlayGameState();
}

// Travel 또는 컨트롤러 종료 시 이전 월드 델리게이트 정리
void UCMClientStageLoadComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (BoundPlayGameState)
    {
        BoundPlayGameState->OnStageLoadRequestChanged.RemoveAll(this);
    }
    if (StageLoadCoordinator)
    {
        StageLoadCoordinator->OnStageStartRequiredFinished.RemoveAll(this);
    }
    Super::EndPlay(EndPlayReason);
}

// Seamless Travel로 GameState가 교체됐는지 확인하고 필요할 때만 재구독
void UCMClientStageLoadComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    TryBindPlayGameState();
}

// 현재 월드 GameState와 GameInstance Coordinator 연결
void UCMClientStageLoadComponent::TryBindPlayGameState()
{
    ACMPlayerController* Controller = Cast<ACMPlayerController>(GetOwner());
    if (!Controller || !Controller->IsLocalController())
    {
        return;
    }

    ACMPlayGameState* PlayGameState = GetWorld()
        ? GetWorld()->GetGameState<ACMPlayGameState>() : nullptr;
    if (BoundPlayGameState == PlayGameState)
    {
        return;
    }
    if (BoundPlayGameState)
    {
        BoundPlayGameState->OnStageLoadRequestChanged.RemoveAll(this);
    }

    UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    StageLoadCoordinator = GameInstance
        ? GameInstance->GetSubsystem<UCMStageLoadCoordinatorSubsystem>() : nullptr;
    BoundPlayGameState = PlayGameState;
    if (!BoundPlayGameState || !StageLoadCoordinator)
    {
        return;
    }

    BoundPlayGameState->OnStageLoadRequestChanged.AddUniqueDynamic(
        this, &ThisClass::HandleStageLoadRequestChanged);
    StageLoadCoordinator->OnStageStartRequiredFinished.AddUniqueDynamic(
        this, &ThisClass::HandleStageStartRequiredFinished);
    HandleStageLoadRequestChanged(BoundPlayGameState->GetStageLoadRequest());
}

// 새로운 요청만 Coordinator에 전달하고 즉시 실패도 서버에 보고
void UCMClientStageLoadComponent::HandleStageLoadRequestChanged(
    const FCMStageLoadRequest& Request)
{
    ACMPlayerController* Controller = Cast<ACMPlayerController>(GetOwner());
    if (!Controller || !Request.IsValid() || Request.RequestId == LastHandledRequestId
        || !StageLoadCoordinator)
    {
        return;
    }

    LastHandledRequestId = Request.RequestId;
    if (!StageLoadCoordinator->StartStageScheduleRequest(
        Request.ScheduleId, Request.RequestId))
    {
        Controller->ReportLocalStageLoadComplete(Request.RequestId, false);
    }
}

// Coordinator의 최종 결과를 PlayerController 서버 RPC 경계에 전달
void UCMClientStageLoadComponent::HandleStageStartRequiredFinished(
    FGuid RequestId,
    bool bSucceeded)
{
    if (ACMPlayerController* Controller = Cast<ACMPlayerController>(GetOwner()))
    {
        Controller->ReportLocalStageLoadComplete(RequestId, bSucceeded);
    }
}
