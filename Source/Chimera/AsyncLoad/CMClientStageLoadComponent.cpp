#include "AsyncLoad/CMClientStageLoadComponent.h"

#include "AsyncLoad/CMStageLoadLog.h"
#include "AsyncLoad/CMStageLoadCoordinatorSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "GameMode/Play/CMPlayGameState.h"
#include "Player/CMPlayerController.h"

namespace
{
bool IsCurrentLocalPlayerController(
    ACMPlayerController* Controller,
    UWorld* World)
{
    ULocalPlayer* LocalPlayer = Controller
        ? Controller->GetLocalPlayer()
        : nullptr;
    return Controller && World && Controller->IsLocalController()
        && LocalPlayer
        && LocalPlayer->GetPlayerController(World) == Controller;
}
}

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
    if (!IsCurrentLocalPlayerController(Controller, GetWorld()))
    {
        if (BoundPlayGameState)
        {
            BoundPlayGameState->OnStageLoadRequestChanged.RemoveAll(this);
            BoundPlayGameState = nullptr;
        }
        if (StageLoadCoordinator)
        {
            StageLoadCoordinator->OnStageStartRequiredFinished.RemoveAll(this);
            StageLoadCoordinator = nullptr;
        }
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

    UE_LOG(LogChimeraStageLoad, Display,
        TEXT("Local stage loader bound. NetMode=%d Controller=%s Request=%s Schedule=%s"),
        static_cast<int32>(GetWorld()->GetNetMode()),
        *GetNameSafe(Controller),
        *BoundPlayGameState->GetStageLoadRequest().RequestId.ToString(),
        *BoundPlayGameState->GetStageLoadRequest().ScheduleId.ToString());
}

// 새로운 요청만 Coordinator에 전달하고 즉시 실패도 서버에 보고
void UCMClientStageLoadComponent::HandleStageLoadRequestChanged(
    const FCMStageLoadRequest& Request)
{
    ACMPlayerController* Controller = Cast<ACMPlayerController>(GetOwner());
    if (!IsCurrentLocalPlayerController(Controller, GetWorld())
        || !Request.IsValid() || Request.RequestId == LastHandledRequestId
        || !StageLoadCoordinator)
    {
        return;
    }

    // 동기 실패 콜백이 GameState 요청을 비워도 같은 ID로 한 번만 보고하도록 복사
    const FGuid RequestId = Request.RequestId;
    const FPrimaryAssetId ScheduleId = Request.ScheduleId;
    LastHandledRequestId = RequestId;

    UE_LOG(LogChimeraStageLoad, Display,
        TEXT("Local stage load request received. NetMode=%d Controller=%s Request=%s Schedule=%s"),
        static_cast<int32>(GetWorld()->GetNetMode()),
        *GetNameSafe(Controller),
        *RequestId.ToString(),
        *ScheduleId.ToString());

    if (!StageLoadCoordinator->StartStageScheduleRequest(
        ScheduleId, RequestId))
    {
        Controller->ReportLocalStageLoadComplete(RequestId, false);
    }
}

// Coordinator의 최종 결과를 PlayerController 서버 RPC 경계에 전달
void UCMClientStageLoadComponent::HandleStageStartRequiredFinished(
    FGuid RequestId,
    bool bSucceeded)
{
    ACMPlayerController* Controller = Cast<ACMPlayerController>(GetOwner());
    if (IsCurrentLocalPlayerController(Controller, GetWorld()))
    {
        Controller->ReportLocalStageLoadComplete(RequestId, bSucceeded);
    }
}
