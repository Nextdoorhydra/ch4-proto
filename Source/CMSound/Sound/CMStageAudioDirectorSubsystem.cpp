#include "Sound/CMStageAudioDirectorSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFlow/CMPlayStateMessages.h"
#include "Sound/NKMSoundSubsystem.h"

void UCMStageAudioDirectorSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    if (InWorld.GetNetMode() == NM_DedicatedServer || !UGameplayMessageSubsystem::HasInstance(this))
    {
        return;
    }

    UGameplayMessageSubsystem& Messages = UGameplayMessageSubsystem::Get(this);
    StateChangedHandle = Messages.RegisterListener<FCMPlayStateMessage>(
        CMPlayStateMessages::StateChanged, this, &ThisClass::HandlePlayStateChanged);

    // 구독을 먼저 완료한 뒤 스냅샷을 요청한다. GameState가 아직 시작 전이면 BeginPlay에서 받는다.
    FCMPlayStateRequest Request;
    Request.World = &InWorld;
    Messages.BroadcastMessage(CMPlayStateMessages::RequestCurrentState, Request);
}

void UCMStageAudioDirectorSubsystem::Deinitialize()
{
    StateChangedHandle.Unregister();
    Super::Deinitialize();
}

void UCMStageAudioDirectorSubsystem::HandlePlayStateChanged(FGameplayTag Channel, const FCMPlayStateMessage& Message)
{
    UWorld* World = GetWorld();
    if (Message.World != World)
    {
        return;
    }
    UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    UNKMSoundSubsystem* SoundSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UNKMSoundSubsystem>() : nullptr;
    if (!SoundSubsystem)
    {
        return;
    }

    switch (Message.Phase)
    {
    case ECMPlayPhase::Starting:
    case ECMPlayPhase::Playing:
        if (Message.StageBGMTag.IsValid())
        {
            SoundSubsystem->PlayBGM(Message.StageBGMTag);
        }
        break;
    case ECMPlayPhase::Completed:
    case ECMPlayPhase::Victory:
    case ECMPlayPhase::Failed:
    case ECMPlayPhase::Defeat:
    case ECMPlayPhase::Ending:
    case ECMPlayPhase::Loading:
        SoundSubsystem->StopBGM();
        break;
    case ECMPlayPhase::WaitingForPlayers:
    default:
        break;
    }
}
