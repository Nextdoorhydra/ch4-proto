#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFlow/CMPlayStateMessages.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameMode/Play/CMPlayGameState.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCMPlayStateEventTest,
    "Chimera.Sound.PlayStateEvents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMPlayStateEventTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    GameInstance->InitializeStandalone();
    UWorld* World = GameInstance->GetWorld();
    UGameplayMessageSubsystem& Messages = UGameplayMessageSubsystem::Get(World);
    ACMPlayGameState* GameState = World->SpawnActor<ACMPlayGameState>();
    FProperty* BGMProperty = FindFProperty<FProperty>(GameState->GetClass(), TEXT("CurrentStageBGMTag"));
    TestNotNull(TEXT("Replicated BGM property exists"), BGMProperty);
    if (BGMProperty)
    {
        TestTrue(TEXT("BGM property is replicated"), BGMProperty->HasAnyPropertyFlags(CPF_Net));
        TestEqual(TEXT("Received BGM notifies local state listeners"), BGMProperty->RepNotifyFunc, FName(TEXT("OnRep_PlayState")));
    }
    int32 ReceivedCount = 0;
    FCMPlayStateMessage LastMessage;
    auto Subscribe = [&]()
    {
        return Messages.RegisterListener<FCMPlayStateMessage>(CMPlayStateMessages::StateChanged,
            [&ReceivedCount, &LastMessage](FGameplayTag, const FCMPlayStateMessage& Message)
            {
                ++ReceivedCount;
                LastMessage = Message;
            });
    };
    FGameplayMessageListenerHandle Listener = Subscribe();
    FCMPlayStateRequest Request;
    Request.World = World;
    Messages.BroadcastMessage(CMPlayStateMessages::RequestCurrentState, Request);
    TestEqual(TEXT("Request before GameState begins has no reply yet"), ReceivedCount, 0);
    GameState->DispatchBeginPlay();
    TestEqual(TEXT("Early subscriber receives initial state at BeginPlay"), ReceivedCount, 1);
    TestEqual(TEXT("Initial phase is Loading"), LastMessage.Phase, ECMPlayPhase::Loading);
    TestTrue(TEXT("Snapshot identifies its world"), LastMessage.World == World);

    GameState->SetPlayPhase(ECMPlayPhase::Playing);
    TestEqual(TEXT("Phase changes broadcast"), LastMessage.Phase, ECMPlayPhase::Playing);
    const FGameplayTag BGMTag = FGameplayTag::RequestGameplayTag(TEXT("Chimera.Sound.BGM.Stage.Stage01"));
    GameState->SetStageProgress(1, 2, BGMTag);
    TestEqual(TEXT("BGM snapshot works without a local stage route"), LastMessage.StageBGMTag, BGMTag);
    TestEqual(TEXT("Stage changes carry current index"), LastMessage.StageIndex, 1);
    Listener.Unregister();
    GameState->SetPlayPhase(ECMPlayPhase::Failed);
    Listener = Subscribe();
    Messages.BroadcastMessage(CMPlayStateMessages::RequestCurrentState, Request);
    TestEqual(TEXT("Late subscriber recovers latest phase"), LastMessage.Phase, ECMPlayPhase::Failed);
    TestEqual(TEXT("Late snapshot preserves current stage"), LastMessage.StageIndex, 1);

    TestEqual(TEXT("Late subscriber receives current BGM"), LastMessage.StageBGMTag, BGMTag);
    const int32 CountBeforeTagChange = ReceivedCount;
    GameState->SetStageProgress(1, 2, FGameplayTag());
    TestEqual(TEXT("Tag-only change broadcasts"), ReceivedCount, CountBeforeTagChange + 1);
    TestFalse(TEXT("Cleared snapshot does not retain old tag"), LastMessage.StageBGMTag.IsValid());
    const int32 CountBeforeForeignRequest = ReceivedCount;
    FCMPlayStateRequest ForeignRequest;
    Messages.BroadcastMessage(CMPlayStateMessages::RequestCurrentState, ForeignRequest);
    TestEqual(TEXT("Another world's request is ignored"), ReceivedCount, CountBeforeForeignRequest);
    GameState->Destroy();
    Messages.BroadcastMessage(CMPlayStateMessages::RequestCurrentState, Request);
    TestEqual(TEXT("Destroyed GameState no longer responds"), ReceivedCount, CountBeforeForeignRequest);
    Listener.Unregister();
    GameInstance->Shutdown();
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    return true;
}

#endif
