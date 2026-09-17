#include "Sound/NKMSoundSubsystem.h"

#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/NKMSoundManager.h"

UAudioComponent* UNKMSoundSubsystem::PlaySFX(FGameplayTag SoundTag, FVector Location)
{
	return PlaySFXWithVolume(SoundTag, Location, 1.0f);
}

UAudioComponent* UNKMSoundSubsystem::PlaySFXForActor(FGameplayTag SoundTag, FVector Location, AActor* SourceActor)
{
	if (!SoundManager || bGameplayAudioFadeActive) return nullptr;

	const FNKMSoundEntry* Entry = FindOrLoadEntry(SoundTag);
	if (!Entry || !SoundManager->CanPlaySound(this, *Entry, &Location, SourceActor)) return nullptr;

	return SpawnSoundAtLocationFromEntry(*Entry, Location, 1.f, 1.f, SourceActor);
}

UAudioComponent* UNKMSoundSubsystem::PlaySFX2D(FGameplayTag SoundTag)
{
	if (!SoundManager || bGameplayAudioFadeActive) return nullptr;

	const FNKMSoundEntry* Entry = FindOrLoadEntry(SoundTag);
	if (!Entry || !SoundManager->CanPlaySound(this, *Entry)) return nullptr;

	return SpawnSound2DFromEntry(*Entry);
}

UAudioComponent* UNKMSoundSubsystem::PlaySFXWithVolume(FGameplayTag SoundTag, FVector Location, float VolumeMultiplier)
{
	if (!SoundManager || bGameplayAudioFadeActive)
	{
		return nullptr;
	}

	const FNKMSoundEntry* Entry = FindOrLoadEntry(SoundTag);
	if (!Entry || !SoundManager->CanPlaySound(this, *Entry, &Location))
	{
		// 등록되지 않은 태그이거나, 쿨다운/거리/동시 재생 제한에 걸린 경우 재생하지 않습니다.
		return nullptr;
	}

	return SpawnSoundAtLocationFromEntry(*Entry, Location, VolumeMultiplier);
}

UAudioComponent* UNKMSoundSubsystem::PlaySFXWithPitch(FGameplayTag SoundTag, FVector Location, float PitchMultiplier)
{
	if (!SoundManager || bGameplayAudioFadeActive) return nullptr;

	const FNKMSoundEntry* Entry = FindOrLoadEntry(SoundTag);
	if (!Entry || !SoundManager->CanPlaySound(this, *Entry, &Location)) return nullptr;

	return SpawnSoundAtLocationFromEntry(*Entry, Location, 1.f, PitchMultiplier);
}

UAudioComponent* UNKMSoundSubsystem::PlayAttachedSFX(FGameplayTag SoundTag, USceneComponent* AttachToComponent, FName SocketName)
{
	return PlayAttachedSFXWithPitch(SoundTag, AttachToComponent, 1.f, SocketName);
}

UAudioComponent* UNKMSoundSubsystem::PlayAttachedSFXWithPitch(FGameplayTag SoundTag, USceneComponent* AttachToComponent, float PitchMultiplier, FName SocketName)
{
	if (!SoundManager || bGameplayAudioFadeActive || !AttachToComponent)
	{
		return nullptr;
	}

	const FVector Location = AttachToComponent->GetComponentLocation();
	const FNKMSoundEntry* Entry = FindOrLoadEntry(SoundTag);
	AActor* SourceActor = AttachToComponent->GetOwner();
	if (!Entry || !SoundManager->CanPlaySound(this, *Entry, &Location, SourceActor))
	{
		// 부착형 사운드도 현재 컴포넌트 위치를 기준으로 거리 제한을 검사합니다.
		return nullptr;
	}

	return SpawnSoundAttachedFromEntry(*Entry, AttachToComponent, SocketName, PitchMultiplier);
}

UAudioComponent* UNKMSoundSubsystem::PlayUISound(FGameplayTag SoundTag)
{
	if (!SoundManager)
	{
		return nullptr;
	}

	const FNKMSoundEntry* Entry = FindOrLoadEntry(SoundTag);
	if (!Entry || !SoundManager->CanPlaySound(this, *Entry))
	{
		// UI 사운드는 위치가 없으므로 거리 제한은 검사하지 않습니다.
		return nullptr;
	}

	return SpawnSound2DFromEntry(*Entry);
}

UAudioComponent* UNKMSoundSubsystem::SpawnSound2DFromEntry(const FNKMSoundEntry& Entry, const bool bAutoDestroy, const float FadeInTime)
{
	// 2D 사운드는 위치 감쇠 없이 전체 화면에서 들립니다.
	USoundBase* Sound = SoundManager ? SoundManager->LoadSound(Entry) : nullptr;
	if (!Sound)
	{
		return nullptr;
	}

	const float TargetVolume = Entry.Volume * GetLayerVolume(Entry.Layer);
	USoundConcurrency* PlaybackConcurrency = SoundManager->LoadConcurrency(Entry);
	if (Entry.Layer == ENKMSoundLayer::BGM && !PlaybackConcurrency)
	{
		if (!DedicatedBGMConcurrency)
		{
			DedicatedBGMConcurrency = NewObject<USoundConcurrency>(this);
			DedicatedBGMConcurrency->Concurrency.MaxCount = 4;
		}

		// 별도 에셋이 없는 BGM도 일반 사운드의 Default Concurrency와 경쟁하지 않게 합니다.
		PlaybackConcurrency = DedicatedBGMConcurrency;
	}

	UAudioComponent* AudioComponent = nullptr;
	if (FadeInTime > 0.f)
	{
		// Create 후 FadeIn해야 첫 프레임부터 0 볼륨으로 시작해 볼륨 튐이 없습니다.
		AudioComponent = UGameplayStatics::CreateSound2D(
			this,
			Sound,
			TargetVolume,
			GetEntryPitch(Entry),
			0.0f,
			PlaybackConcurrency,
			false,
			bAutoDestroy);

		if (AudioComponent)
		{
			AudioComponent->FadeIn(FadeInTime, 1.f, 0.f, EAudioFaderCurve::Sin);
		}
	}
	else
	{
		AudioComponent = UGameplayStatics::SpawnSound2D(
			this,
			Sound,
			TargetVolume,
			GetEntryPitch(Entry),
			0.0f,
			PlaybackConcurrency,
			false,
			bAutoDestroy);
	}

	if (SoundManager)
	{
		// 쿨다운/동시 재생 제한을 위해 실제 재생된 컴포넌트를 기록합니다.
		ApplyEntryTimeDilationEffects(AudioComponent, Entry);
		SoundManager->NotifySoundStarted(this, Entry, AudioComponent);
	}

	return AudioComponent;
}

UAudioComponent* UNKMSoundSubsystem::SpawnSoundAtLocationFromEntry(const FNKMSoundEntry& Entry, const FVector& Location, float VolumeMultiplier, float PitchMultiplier, AActor* SourceActor)
{
	// 3D 위치 사운드는 감쇠/동시 재생 에셋을 함께 적용합니다.
	USoundBase* Sound = SoundManager ? SoundManager->LoadSound(Entry) : nullptr;
	if (!Sound)
	{
		return nullptr;
	}

	UAudioComponent* AudioComponent = UGameplayStatics::SpawnSoundAtLocation(this, Sound, Location, FRotator::ZeroRotator, Entry.Volume * GetLayerVolume(Entry.Layer) * FMath::Max(0.0f, VolumeMultiplier), GetEntryPitch(Entry) * FMath::Max(0.01f, PitchMultiplier), 0.0f, SoundManager->LoadAttenuation(Entry), SoundManager->LoadConcurrency(Entry), true);

	if (SoundManager)
	{
		ApplyEntryTimeDilationEffects(AudioComponent, Entry, PitchMultiplier);
		SoundManager->NotifySoundStarted(this, Entry, AudioComponent, PitchMultiplier, SourceActor);
	}

	return AudioComponent;
}

UAudioComponent* UNKMSoundSubsystem::SpawnSoundAttachedFromEntry(const FNKMSoundEntry& Entry, USceneComponent* AttachToComponent, FName SocketName, float PitchMultiplier)
{
	// 부착형 사운드는 AttachToComponent를 따라 움직입니다.
	// 예: 총구 소켓, 캐릭터 몸통, 움직이는 투사체 등에 붙일 때 사용합니다.
	USoundBase* Sound = SoundManager ? SoundManager->LoadSound(Entry) : nullptr;
	if (!Sound || !AttachToComponent)
	{
		return nullptr;
	}

	UAudioComponent* AudioComponent = UGameplayStatics::SpawnSoundAttached(
		Sound,
		AttachToComponent,
		SocketName,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget,
		false,
		Entry.Volume * GetLayerVolume(Entry.Layer),
		GetEntryPitch(Entry) * FMath::Max(0.01f, PitchMultiplier),
		0.0f,
		SoundManager->LoadAttenuation(Entry),
		SoundManager->LoadConcurrency(Entry),
		true);

	if (SoundManager)
	{
		ApplyEntryTimeDilationEffects(AudioComponent, Entry, PitchMultiplier);
		SoundManager->NotifySoundStarted(this, Entry, AudioComponent, PitchMultiplier, AttachToComponent->GetOwner());
	}

	return AudioComponent;
}
