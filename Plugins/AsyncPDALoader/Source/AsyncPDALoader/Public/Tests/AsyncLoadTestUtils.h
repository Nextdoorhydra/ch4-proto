#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AsyncLoadCompleteMessage.h"
#include "AsyncPDALoaderTags.h"
#include "GameFramework/GameplayMessageSubsystem.h"

// 비동기 로드 완료 메시지를 기다리는 테스트용 helper다.
class FAsyncPDALoaderCompletionWaiter
{
public:
	// 완료 메시지 구독과 timeout 측정을 시작한다.
	void Begin(
		UGameInstance* InGameInstance,
		float InTimeoutSeconds = 10.f,
		FGameplayTag InExpectedTimingTag = FGameplayTag(),
		FGuid InExpectedCorrelationId = FGuid())
	{
		GameInstance = InGameInstance;
		TimeoutSeconds = InTimeoutSeconds;
		StartTime = FPlatformTime::Seconds();
		ExpectedTimingTag = InExpectedTimingTag;
		ExpectedCorrelationId = InExpectedCorrelationId;

		if (UGameInstance* GI = GameInstance.Get())
		{
			ListenerHandle = UGameplayMessageSubsystem::Get(GI).RegisterListener<FAsyncLoadCompleteMessage>(
				AsyncPDALoaderTags::Message_Load_Complete,
				[this](FGameplayTag, const FAsyncLoadCompleteMessage& Msg)
				{
					if (ExpectedTimingTag.IsValid() && !Msg.TimingTag.MatchesTagExact(ExpectedTimingTag))
					{
						return;
					}
					if (ExpectedCorrelationId.IsValid() && Msg.CorrelationId != ExpectedCorrelationId)
					{
						return;
					}
					bReceived = true;
					ReceivedMessage = Msg;
				});
		}
	}

	// 완료 또는 timeout이면 true를 반환한다.
	bool Poll(FAsyncLoadCompleteMessage& OutMessage, bool& bOutTimedOut)
	{
		UGameInstance* GI = GameInstance.Get();
		if (!GI)
		{
			bOutTimedOut = true;
			return true;
		}

		if (bReceived)
		{
			UGameplayMessageSubsystem::Get(GI).UnregisterListener(ListenerHandle);
			OutMessage = ReceivedMessage;
			bOutTimedOut = false;
			return true;
		}

		if (FPlatformTime::Seconds() - StartTime > TimeoutSeconds)
		{
			UGameplayMessageSubsystem::Get(GI).UnregisterListener(ListenerHandle);
			bOutTimedOut = true;
			return true;
		}

		return false;
	}

private:
	TWeakObjectPtr<UGameInstance> GameInstance;
	float TimeoutSeconds = 10.f;
	double StartTime = 0.0;
	FGameplayTag ExpectedTimingTag;
	FGuid ExpectedCorrelationId;

	bool bReceived = false;
	FGameplayMessageListenerHandle ListenerHandle;
	FAsyncLoadCompleteMessage ReceivedMessage;
};

#endif // WITH_DEV_AUTOMATION_TESTS

