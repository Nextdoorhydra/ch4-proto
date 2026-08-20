#include "Stage/Obstacle/Component/CMObstacleDefinitionComponent.h"

#include "AsyncLoad/CMStageLoadCoordinatorSubsystem.h"
#include "AsyncLoad/CMStageLoadLog.h"
#include "Stage/Obstacle/Data/CMObstacleDefinition.h"

UCMObstacleDefinitionComponent::UCMObstacleDefinitionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// 로컬 Coordinator의 그룹 완료 알림을 구독하고 현재 준비 상태 확인
void UCMObstacleDefinitionComponent::BeginPlay()
{
    Super::BeginPlay();

    if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
    {
        if (UCMStageLoadCoordinatorSubsystem* Coordinator =
            GameInstance->GetSubsystem<UCMStageLoadCoordinatorSubsystem>())
        {
            Coordinator->OnLoadGroupFinished.AddUniqueDynamic(
                this, &ThisClass::HandleLoadGroupFinished);
        }
    }
    RefreshDefinitionState();
}

// 월드 종료 시 Coordinator 델리게이트 연결 해제
void UCMObstacleDefinitionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
    {
        if (UCMStageLoadCoordinatorSubsystem* Coordinator =
            GameInstance->GetSubsystem<UCMStageLoadCoordinatorSubsystem>())
        {
            Coordinator->OnLoadGroupFinished.RemoveAll(this);
        }
    }
    Super::EndPlay(EndPlayReason);
}

// 비동기 로더가 준비한 Definition을 동기 로드 없이 반환
UCMObstacleDefinition* UCMObstacleDefinitionComponent::GetLoadedDefinition() const
{
    return bDefinitionReady ? Definition.Get() : nullptr;
}

// 레거시 장애물 또는 이미 완료된 LoadGroup 상태를 즉시 반영
void UCMObstacleDefinitionComponent::RefreshDefinitionState()
{
    if (!HasDefinition() || bDefinitionReady || bDefinitionFailed)
    {
        return;
    }
    if (LoadGroupId.IsNone())
    {
        MarkDefinitionFailed(TEXT("LoadGroupId is empty"));
        return;
    }

    UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    const UCMStageLoadCoordinatorSubsystem* Coordinator = GameInstance
        ? GameInstance->GetSubsystem<UCMStageLoadCoordinatorSubsystem>() : nullptr;
    if (!Coordinator)
    {
        MarkDefinitionFailed(TEXT("StageLoadCoordinator is missing"));
        return;
    }

    const ECMStageLoadGroupState State = Coordinator->GetLoadGroupState(LoadGroupId);
    if (State == ECMStageLoadGroupState::Ready)
    {
        TryResolveLoadedDefinition();
    }
    else if (State == ECMStageLoadGroupState::Failed || State == ECMStageLoadGroupState::Released)
    {
        MarkDefinitionFailed(TEXT("LoadGroup is not available"));
    }
}

// 지정된 LoadGroup 성공 시 Soft Definition이 실제로 준비됐는지 검증
void UCMObstacleDefinitionComponent::HandleLoadGroupFinished(
    FName FinishedLoadGroupId,
    EAsyncLoadResult Result,
    bool bReleasedImmediately)
{
    if (FinishedLoadGroupId != LoadGroupId || bDefinitionReady || bDefinitionFailed)
    {
        return;
    }
    if (Result != EAsyncLoadResult::Succeeded || bReleasedImmediately)
    {
        MarkDefinitionFailed(TEXT("LoadGroup failed or was released immediately"));
        return;
    }
    TryResolveLoadedDefinition();
}

// 이미 로드된 Soft Object만 조회해 성공 이벤트 전달
bool UCMObstacleDefinitionComponent::TryResolveLoadedDefinition()
{
    UCMObstacleDefinition* LoadedDefinition = Definition.Get();
    if (!LoadedDefinition)
    {
        MarkDefinitionFailed(TEXT("Definition was not loaded by the assigned LoadGroup"));
        return false;
    }

    bDefinitionReady = true;
    bDefinitionFailed = false;
    OnDefinitionReady.Broadcast(LoadedDefinition);
    return true;
}

// 동기 로드로 우회하지 않고 장애물을 실패 상태로 고정해 잘못된 데이터 노출
void UCMObstacleDefinitionComponent::MarkDefinitionFailed(const TCHAR* Reason)
{
    if (bDefinitionFailed)
    {
        return;
    }

    bDefinitionFailed = true;
    bDefinitionReady = false;
    UE_LOG(LogChimeraStageLoad, Error,
        TEXT("Obstacle Definition failed. Actor=%s Definition=%s LoadGroup=%s Reason=%s"),
        *GetNameSafe(GetOwner()),
        *Definition.ToSoftObjectPath().ToString(),
        *LoadGroupId.ToString(),
        Reason);
    OnDefinitionFailed.Broadcast();
}
