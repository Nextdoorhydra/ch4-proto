#include "Sound/NKMSoundSubsystem.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/NKMSoundManager.h"
#include "Sound/NKMSoundSettings.h"
#include "TimerManager.h"

void UNKMSoundSubsystem::PlayCrowdLoop(FGameplayTag SoundTag)
{
	if (!SoundManager || bGameplayAudioFadeActive || (CurrentCrowdTag.MatchesTagExact(SoundTag) && CrowdAudioComponent && CrowdAudioComponent->IsPlaying()))
	{
		// 같은 군중 루프가 이미 재생 중이면 중복 재생하지 않습니다.
		return;
	}

	const FNKMSoundEntry* Entry = FindOrLoadEntry(SoundTag);
	if (!Entry)
	{
		return;
	}

	const UNKMSoundSettings* SoundSettings = GetDefault<UNKMSoundSettings>();
	const float FadeTime = SoundSettings ? SoundSettings->CrowdFadeTime : 0.5f;

	// 군중 루프는 BGM과 별개로 유지합니다.
	// 강도가 변하면 SetCrowdIntensity -> ApplyPersistentVolumes로 볼륨만 갱신합니다.
	StopCrowdLoop(FadeTime);
	CrowdAudioComponent = SpawnSound2DFromEntry(*Entry, false, FadeTime);
	CurrentCrowdEntryVolume = Entry->Volume;
	CurrentCrowdTag = SoundTag;
	ApplyPersistentVolumes();
}

void UNKMSoundSubsystem::StopCrowdLoop(float FadeOutTime)
{
	if (CrowdAudioComponent)
	{
		CrowdAudioComponent->FadeOut(FadeOutTime, 0.0f);
		CrowdAudioComponent = nullptr;
	}

	CurrentCrowdTag = FGameplayTag();
	CurrentCrowdEntryVolume = 1.0f;
}

void UNKMSoundSubsystem::SetCrowdIntensity(float Intensity)
{
	// 호출자가 계산한 정규화된 강도를 적용합니다.
	CrowdIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
	ApplyPersistentVolumes();
}

void UNKMSoundSubsystem::SetMasterVolume(float Volume)
{
	MasterVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	ApplyPersistentVolumes();
}

void UNKMSoundSubsystem::SetBGMVolume(float Volume)
{
	BGMVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	ApplyPersistentVolumes();
}

void UNKMSoundSubsystem::SetSFXVolume(float Volume)
{
	SFXVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	ApplyPersistentVolumes();
}

void UNKMSoundSubsystem::SetCrowdVolume(float Volume)
{
	CrowdVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	ApplyPersistentVolumes();
}

void UNKMSoundSubsystem::SaveVolumeSettings()
{
	VolumeSaveRequested.Broadcast(MasterVolume, BGMVolume, SFXVolume);
}

void UNKMSoundSubsystem::ApplyTimeDilationAudioTransition()
{
	const UNKMSoundSettings* Settings = GetDefault<UNKMSoundSettings>();
	if (!Settings || !GetWorld()) return;

	const float WorldDilation = UGameplayStatics::GetGlobalTimeDilation(this);
	PitchTransitionTarget = FMath::Clamp(WorldDilation, Settings->MinimumTimeDilationPitch, 1.0f);
	PitchTransitionDuration = PitchTransitionTarget < 1.0f - KINDA_SMALL_NUMBER
		? Settings->SlowMotionPitchFadeInTime
		: Settings->SlowMotionPitchFadeOutTime;
	PitchTransitionStart = CurrentTimeDilationPitch;
	BGMVolumeTransitionStart = CurrentTimeDilationBGMVolume;
	BGMVolumeTransitionTarget = PitchTransitionTarget < 1.0f - KINDA_SMALL_NUMBER
		? Settings->SlowMotionBGMVolumeMultiplier
		: 1.0f;
	NormalLowPassCutoffFrequency = Settings->NormalLowPassCutoffFrequency;
	LowPassTransitionStart = CurrentLowPassCutoffFrequency;
	LowPassTransitionTarget = PitchTransitionTarget < 1.0f - KINDA_SMALL_NUMBER
		? Settings->SlowMotionLowPassCutoffFrequency
		: NormalLowPassCutoffFrequency;
	PitchTransitionElapsed = 0.0f;

	StopTimeDilationAudioTicker();
	if (PitchTransitionDuration <= KINDA_SMALL_NUMBER)
	{
		CurrentTimeDilationPitch = PitchTransitionTarget;
		CurrentTimeDilationBGMVolume = BGMVolumeTransitionTarget;
		CurrentLowPassCutoffFrequency = LowPassTransitionTarget;
		if (SoundManager)
		{
			SoundManager->ApplyTimeDilationEffects(
				CurrentTimeDilationPitch,
				CurrentLowPassCutoffFrequency,
				CurrentLowPassCutoffFrequency < NormalLowPassCutoffFrequency - 1.0f);
		}
		ApplyPersistentVolumes();
		return;
	}

	TimeDilationAudioTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UNKMSoundSubsystem::TickTimeDilationAudioTransition));
}

float UNKMSoundSubsystem::GetEntryPitch(const FNKMSoundEntry& Entry) const
{
	return Entry.Layer == ENKMSoundLayer::UI
		? Entry.Pitch
		: Entry.Pitch * CurrentTimeDilationPitch;
}

void UNKMSoundSubsystem::ApplyEntryTimeDilationEffects(UAudioComponent* AudioComponent, const FNKMSoundEntry& Entry, float PitchMultiplier) const
{
	if (!AudioComponent) return;

	AudioComponent->SetPitchMultiplier(GetEntryPitch(Entry) * FMath::Max(0.01f, PitchMultiplier));
	const bool bIsLowPassLayer =
		Entry.Layer == ENKMSoundLayer::BGM ||
		Entry.Layer == ENKMSoundLayer::Character ||
		Entry.Layer == ENKMSoundLayer::Crowd;
	const bool bEnableLowPass = bIsLowPassLayer && CurrentLowPassCutoffFrequency < NormalLowPassCutoffFrequency - 1.0f;
	AudioComponent->SetLowPassFilterEnabled(bEnableLowPass);
	if (bIsLowPassLayer)
	{
		AudioComponent->SetLowPassFilterFrequency(CurrentLowPassCutoffFrequency);
	}
}

bool UNKMSoundSubsystem::TickTimeDilationAudioTransition(float DeltaTime)
{
	PitchTransitionElapsed += FMath::Max(0.0f, DeltaTime);
	const float Alpha = FMath::Clamp(PitchTransitionElapsed / PitchTransitionDuration, 0.0f, 1.0f);
	CurrentTimeDilationPitch = FMath::InterpEaseInOut(
		PitchTransitionStart, PitchTransitionTarget, Alpha, 2.0f);
	CurrentTimeDilationBGMVolume = FMath::InterpEaseInOut(
		BGMVolumeTransitionStart, BGMVolumeTransitionTarget, Alpha, 2.0f);
	CurrentLowPassCutoffFrequency = FMath::InterpEaseInOut(
		LowPassTransitionStart, LowPassTransitionTarget, Alpha, 2.0f);

	if (SoundManager)
	{
		SoundManager->ApplyTimeDilationEffects(
			CurrentTimeDilationPitch,
			CurrentLowPassCutoffFrequency,
			CurrentLowPassCutoffFrequency < NormalLowPassCutoffFrequency - 1.0f);
	}
	ApplyPersistentVolumes();

	if (Alpha >= 1.0f)
	{
		TimeDilationAudioTickerHandle.Reset();
		return false;
	}
	return true;
}

void UNKMSoundSubsystem::StopTimeDilationAudioTicker()
{
	if (TimeDilationAudioTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TimeDilationAudioTickerHandle);
		TimeDilationAudioTickerHandle.Reset();
	}
}

float UNKMSoundSubsystem::GetLayerVolume(ENKMSoundLayer Layer) const
{
	// 현재는 코드 레벨 볼륨 배율만 계산합니다.
	// 나중에 SoundClass/SoundMix/Audio Modulation까지 붙이면 이 함수 또는 SetVolume 계열에서 연결하면 됩니다.
	switch (Layer)
	{
	case ENKMSoundLayer::BGM:
		return MasterVolume * BGMVolume * CurrentTimeDilationBGMVolume;
	case ENKMSoundLayer::Crowd:
		// Crowd 레이어는 환호 같은 이벤트성 사운드에도 사용하므로 Intensity를 공통 배율에 넣지 않는다.
		return MasterVolume * CrowdVolume * DuckingVolumeMultiplier;
	case ENKMSoundLayer::Character:
	case ENKMSoundLayer::Voice:
		return MasterVolume * SFXVolume * DuckingVolumeMultiplier;
	case ENKMSoundLayer::SFX:
	case ENKMSoundLayer::UI:
	default:
		return MasterVolume * SFXVolume;
	}
}

void UNKMSoundSubsystem::BeginDucking(float Duration, float VolumeMultiplier)
{
	UWorld* World = GetWorld();
	if (!World || !SoundManager || Duration <= 0.f) return;

	DuckingVolumeMultiplier = FMath::Clamp(VolumeMultiplier, 0.01f, 1.f);
	ApplyPersistentVolumes();

	World->GetTimerManager().ClearTimer(DuckingTimerHandle);
	World->GetTimerManager().SetTimer(DuckingTimerHandle, this, &ThisClass::EndDucking, Duration, false);
}

void UNKMSoundSubsystem::EndDucking()
{
	DuckingVolumeMultiplier = 1.f;
	ApplyPersistentVolumes();
	DuckingTimerHandle.Invalidate();
}

void UNKMSoundSubsystem::ApplyPersistentVolumes()
{
	// BGM과 Crowd는 한 번 재생한 뒤 계속 유지되는 AudioComponent라,
	// 옵션 값이 바뀔 때 현재 컴포넌트의 VolumeMultiplier를 갱신해줘야 합니다.
	if (BGMAudioComponent)
	{
		BGMAudioComponent->SetVolumeMultiplier(CurrentBGMEntryVolume * GetLayerVolume(ENKMSoundLayer::BGM));
	}
	if (SuspendedBGMAudioComponent)
	{
		SuspendedBGMAudioComponent->SetVolumeMultiplier(SuspendedBGMEntryVolume * GetLayerVolume(ENKMSoundLayer::BGM));
	}

	if (CrowdAudioComponent)
	{
		// CrowdIntensity는 폐기 예정인 지속 군중 루프에만 적용한다.
		// 이벤트성 Cheer는 각 AudioComponent 생성 시 CrowdVolume만 사용한다.
		CrowdAudioComponent->SetVolumeMultiplier(
			CurrentCrowdEntryVolume * GetLayerVolume(ENKMSoundLayer::Crowd) * CrowdIntensity);
	}

	if (SoundManager)
	{
		SoundManager->PruneFinishedSounds();
	}
}
