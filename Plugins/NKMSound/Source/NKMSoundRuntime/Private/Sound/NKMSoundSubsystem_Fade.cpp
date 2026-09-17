#include "Sound/NKMSoundSubsystem.h"

#include "Sound/NKMSoundManager.h"

void UNKMSoundSubsystem::BeginGameplayAudioFade(float FadeDuration, float LowPassCutoffFrequency)
{
	if (!SoundManager) return;

	bGameplayAudioFadeActive = true;
	StopTimeDilationAudioTicker();
	GameplayAudioFadeElapsed = 0.0f;
	GameplayAudioFadeDuration = FMath::Max(0.0f, FadeDuration);
	GameplayLowPassStart = CurrentLowPassCutoffFrequency;
	GameplayLowPassTarget = FMath::Clamp(LowPassCutoffFrequency, 20.0f, NormalLowPassCutoffFrequency);

	SoundManager->FadeOutGameplaySounds(GameplayAudioFadeDuration);
	StopGameplayAudioFadeTicker();

	if (GameplayAudioFadeDuration <= KINDA_SMALL_NUMBER)
	{
		SoundManager->ApplyGameplayLowPass(GameplayLowPassTarget, true);
		return;
	}

	GameplayAudioFadeTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::TickGameplayAudioFade));
}

void UNKMSoundSubsystem::ResetGameplayAudioFade()
{
	bGameplayAudioFadeActive = false;
	StopGameplayAudioFadeTicker();
	StopTimeDilationAudioTicker();
	CurrentTimeDilationPitch = 1.0f;
	CurrentTimeDilationBGMVolume = 1.0f;
	CurrentLowPassCutoffFrequency = NormalLowPassCutoffFrequency;

	StopBGM(0.0f);
	StopCrowdLoop(0.0f);

	if (SoundManager)
	{
		SoundManager->ApplyTimeDilationEffects(
			CurrentTimeDilationPitch,
			CurrentLowPassCutoffFrequency,
			CurrentLowPassCutoffFrequency < NormalLowPassCutoffFrequency - 1.0f);
		SoundManager->ResetRuntimeState();
	}
}

bool UNKMSoundSubsystem::TickGameplayAudioFade(float DeltaTime)
{
	GameplayAudioFadeElapsed += FMath::Max(0.0f, DeltaTime);
	const float Alpha = FMath::Clamp(GameplayAudioFadeElapsed / GameplayAudioFadeDuration, 0.0f, 1.0f);
	const float CutoffFrequency = FMath::InterpEaseInOut(GameplayLowPassStart, GameplayLowPassTarget, Alpha, 2.0f);

	if (SoundManager)
	{
		SoundManager->ApplyGameplayLowPass(CutoffFrequency, true);
	}

	if (Alpha >= 1.0f)
	{
		GameplayAudioFadeTickerHandle.Reset();
		return false;
	}

	return true;
}

void UNKMSoundSubsystem::StopGameplayAudioFadeTicker()
{
	if (GameplayAudioFadeTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(GameplayAudioFadeTickerHandle);
		GameplayAudioFadeTickerHandle.Reset();
	}
}
