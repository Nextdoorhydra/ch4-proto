#include "Sound/NKMSoundSubsystem.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Sound/NKMSoundManager.h"
#include "Sound/NKMSoundSettings.h"

namespace
{
	// AdjustVolume의 목표값을 0으로 지정하면 컴포넌트가 종료되므로, 일시정지 전에는 들리지 않을 정도의 최소값까지만 낮춥니다.
	constexpr float NKMSilentFadeLevel = 0.001f;
}

void UNKMSoundSubsystem::PlayBGM(FGameplayTag SoundTag, bool bRestartIfSame)
{
	if (!SoundManager || (!bRestartIfSame && CurrentBGMTag.MatchesTagExact(SoundTag) && BGMAudioComponent))
	{
		// 같은 BGM이 이미 재생 중이면 기본적으로 다시 시작하지 않습니다.
		return;
	}

	const FNKMSoundEntry* Entry = FindOrLoadEntry(SoundTag);
	if (!Entry)
	{
		return;
	}

	const float FadeTime = GetBGMTransitionTime();

	// 기존 BGM을 페이드아웃하고 새 BGM을 2D 사운드로 재생합니다.
	StopBGM(FadeTime);
	StartBGM(SoundTag, FadeTime);
}

void UNKMSoundSubsystem::PlayBattleBGM(FGameplayTag SoundTag)
{
	if (!SoundManager || !SoundTag.IsValid()) return;
	if (CurrentBGMTag.MatchesTagExact(SoundTag) && BGMAudioComponent) return;
	if (!FindOrLoadEntry(SoundTag) && !SuspendedBGMTag.MatchesTagExact(SoundTag)) return;

	const float FadeTime = GetBGMTransitionTime();
	if (ResumeSuspendedBGM(SoundTag, FadeTime))
	{
		return;
	}

	// 웨이브 구간이 달라졌다면 이전 곡은 폐기하고 새 배틀 BGM을 처음부터 시작한다.
	StopBGM(FadeTime);
	StartBGM(SoundTag, FadeTime);
}

void UNKMSoundSubsystem::PlayIntermissionBGM(FGameplayTag SoundTag)
{
	if (!SoundManager || !SoundTag.IsValid()) return;
	if (CurrentBGMTag.MatchesTagExact(SoundTag) && BGMAudioComponent) return;

	const float FadeTime = GetBGMTransitionTime();

	if (!SuspendedBGMAudioComponent && BGMAudioComponent)
	{
		// 볼륨을 먼저 내린 뒤 Pause해서 다음 전투에서 정확한 재생 위치를 이어갑니다.
		SuspendActiveBGM(FadeTime);
	}
	else
	{
		StopActiveBGM(FadeTime);
	}

	// 정비 BGM 항목을 비워두면 배틀 BGM만 일시정지하고 무음 정비 시간을 유지합니다.
	if (FindOrLoadEntry(SoundTag))
	{
		StartBGM(SoundTag, FadeTime);
	}
}

void UNKMSoundSubsystem::StopBGM(float FadeOutTime)
{
	StopActiveBGM(FadeOutTime);
	ClearSuspendedBGM();
}

void UNKMSoundSubsystem::SetBGMGamePaused(bool bPaused)
{
	if (bBGMGamePaused == bPaused)
	{
		return;
	}

	bBGMGamePaused = bPaused;
	if (BGMAudioComponent)
	{
		BGMAudioComponent->SetPaused(bPaused);
	}
}

float UNKMSoundSubsystem::GetBGMTransitionTime() const
{
	const UNKMSoundSettings* SoundSettings = GetDefault<UNKMSoundSettings>();
	return FMath::Max(0.f, SoundSettings ? SoundSettings->BGMCrossfadeTime : 1.f);
}

bool UNKMSoundSubsystem::StartBGM(FGameplayTag SoundTag, float FadeInTime)
{
	if (bGameplayAudioFadeActive) return false;

	const FNKMSoundEntry* Entry = FindOrLoadEntry(SoundTag);
	if (!Entry) return false;

	// BGM은 옵션에서 볼륨이 0이 되어도 같은 컴포넌트가 계속 재생되어야 한다.
	BGMAudioComponent = SpawnSound2DFromEntry(*Entry, false, FadeInTime);
	if (!BGMAudioComponent) return false;

	CurrentBGMEntryVolume = Entry->Volume;
	CurrentBGMTag = SoundTag;
	ApplyPersistentVolumes();
	if (bBGMGamePaused)
	{
		BGMAudioComponent->SetPaused(true);
	}
	return true;
}

bool UNKMSoundSubsystem::ResumeSuspendedBGM(FGameplayTag SoundTag, float FadeInTime)
{
	if (bGameplayAudioFadeActive) return false;

	if (!SuspendedBGMTag.MatchesTagExact(SoundTag) || !SuspendedBGMAudioComponent)
	{
		return false;
	}

	// 정비 BGM은 내리고, 보관했던 배틀 BGM은 같은 재생 위치에서 다시 올립니다.
	StopActiveBGM(FadeInTime);
	ClearBGMSuspendTimer();

	BGMAudioComponent = SuspendedBGMAudioComponent;
	CurrentBGMTag = SuspendedBGMTag;
	CurrentBGMEntryVolume = SuspendedBGMEntryVolume;

	SuspendedBGMAudioComponent = nullptr;
	SuspendedBGMTag = FGameplayTag();
	SuspendedBGMEntryVolume = 1.0f;

	// Pause 중에는 활성 컴포넌트 목록에서 빠질 수 있으므로, Resume 직전에 현재 피치를 다시 맞춘다.
	if (SoundManager)
	{
		if (const FNKMSoundEntry* Entry = FindOrLoadEntry(CurrentBGMTag))
		{
			ApplyEntryTimeDilationEffects(BGMAudioComponent, *Entry);
		}
	}

	ApplyPersistentVolumes();
	BGMAudioComponent->SetPaused(bBGMGamePaused);
	BGMAudioComponent->AdjustVolume(FadeInTime, 1.f, EAudioFaderCurve::Sin);
	return true;
}

void UNKMSoundSubsystem::SuspendActiveBGM(float FadeOutTime)
{
	if (!BGMAudioComponent) return;

	ClearBGMSuspendTimer();
	SuspendedBGMAudioComponent = BGMAudioComponent;
	SuspendedBGMTag = CurrentBGMTag;
	SuspendedBGMEntryVolume = CurrentBGMEntryVolume;

	BGMAudioComponent = nullptr;
	CurrentBGMTag = FGameplayTag();
	CurrentBGMEntryVolume = 1.0f;

	if (FadeOutTime <= 0.f)
	{
		PauseSuspendedBGM();
		return;
	}

	// 정확히 0까지 내리면 UE가 컴포넌트를 정지하므로 최소 볼륨에서 Pause합니다.
	SuspendedBGMAudioComponent->AdjustVolume(FadeOutTime, NKMSilentFadeLevel, EAudioFaderCurve::Sin);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(BGMSuspendTimerHandle, this, &ThisClass::PauseSuspendedBGM, FadeOutTime, false);
	}
	else
	{
		PauseSuspendedBGM();
	}
}

void UNKMSoundSubsystem::PauseSuspendedBGM()
{
	BGMSuspendTimerHandle.Invalidate();
	if (SuspendedBGMAudioComponent)
	{
		SuspendedBGMAudioComponent->SetPaused(true);
	}
}

void UNKMSoundSubsystem::ClearBGMSuspendTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BGMSuspendTimerHandle);
	}
	BGMSuspendTimerHandle.Invalidate();
}

void UNKMSoundSubsystem::StopActiveBGM(float FadeOutTime)
{
	if (BGMAudioComponent)
	{
		BGMAudioComponent->FadeOut(FadeOutTime, 0.0f, EAudioFaderCurve::Sin);
		BGMAudioComponent = nullptr;
	}

	CurrentBGMTag = FGameplayTag();
	CurrentBGMEntryVolume = 1.0f;
}

void UNKMSoundSubsystem::ClearSuspendedBGM()
{
	ClearBGMSuspendTimer();
	if (SuspendedBGMAudioComponent)
	{
		SuspendedBGMAudioComponent->Stop();
		SuspendedBGMAudioComponent = nullptr;
	}

	SuspendedBGMTag = FGameplayTag();
	SuspendedBGMEntryVolume = 1.0f;
}
