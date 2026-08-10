#include "Sound/NKMSoundManager.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/NKMSoundCatalogProvider.h"

void UNKMSoundManager::Initialize(const TArray<UObject*>& SoundCatalogs)
{
	// 데이터 에셋을 다시 읽을 때 기존 테이블을 남겨두면 삭제된 태그가 계속 살아있을 수 있으므로 먼저 비웁니다.
	EntriesByTag.Empty();
	TMap<FGameplayTag, FPrimaryAssetId> SourceAssetIdsByTag;

	for (const UObject* SoundCatalog : SoundCatalogs)
	{
		const INKMSoundCatalogProvider* Provider = Cast<INKMSoundCatalogProvider>(SoundCatalog);
		if (!Provider)
		{
			continue;
		}

		const FPrimaryAssetId SourceAssetId = Provider->GetSoundCatalogId();
		for (const FNKMSoundEntry& Entry : Provider->GetSoundEntries())
		{
			if (!Entry.SoundTag.IsValid())
			{
				continue;
			}

			if (const FPrimaryAssetId* ExistingAssetId = SourceAssetIdsByTag.Find(Entry.SoundTag))
			{
				UE_LOG(LogTemp, Error,
					TEXT("[NKMSound] 사운드 태그가 중복되었습니다. 먼저 등록된 항목을 유지합니다. 사운드태그=%s 최초PDA=%s 중복PDA=%s"),
					*Entry.SoundTag.ToString(), *ExistingAssetId->ToString(), *SourceAssetId.ToString());
				continue;
			}

			SourceAssetIdsByTag.Add(Entry.SoundTag, SourceAssetId);
			EntriesByTag.Add(Entry.SoundTag, Entry);
		}
	}
}

void UNKMSoundManager::ResetRuntimeState()
{
	EndSourceIsolation();
	LastPlayTimeByTag.Empty();
	ActiveComponentsByTag.Empty();
}

const FNKMSoundEntry* UNKMSoundManager::FindEntry(FGameplayTag SoundTag) const
{
	return SoundTag.IsValid() ? EntriesByTag.Find(SoundTag) : nullptr;
}

USoundBase* UNKMSoundManager::LoadSound(const FNKMSoundEntry& Entry) const
{
	if (Entry.Sound.IsNull()) return nullptr;
	if (USoundBase* LoadedSound = Entry.Sound.Get()) return LoadedSound;

#if 0
	UE_LOG(LogTemp, Error,
		TEXT("[NKMSound] 사운드 PDA의 Gameplay 번들로 사운드가 로드되지 않아 동기 강제 로드합니다. 사운드태그=%s 에셋=%s"),
		*Entry.SoundTag.ToString(), *Entry.Sound.ToSoftObjectPath().ToString());
#endif
	UE_LOG(LogTemp, Error,
		TEXT("[NKMSound] Sound asset was not preloaded; playback was skipped. SoundTag=%s Asset=%s"),
		*Entry.SoundTag.ToString(), *Entry.Sound.ToSoftObjectPath().ToString());
	return nullptr;
}

USoundAttenuation* UNKMSoundManager::LoadAttenuation(const FNKMSoundEntry& Entry) const
{
	if (Entry.Attenuation.IsNull()) return nullptr;
	if (USoundAttenuation* LoadedAttenuation = Entry.Attenuation.Get()) return LoadedAttenuation;

#if 0
	UE_LOG(LogTemp, Error,
		TEXT("[NKMSound] 사운드 PDA의 Gameplay 번들로 감쇠 설정이 로드되지 않아 동기 강제 로드합니다. 사운드태그=%s 에셋=%s"),
		*Entry.SoundTag.ToString(), *Entry.Attenuation.ToSoftObjectPath().ToString());
#endif
	UE_LOG(LogTemp, Error,
		TEXT("[NKMSound] Attenuation asset was not preloaded. SoundTag=%s Asset=%s"),
		*Entry.SoundTag.ToString(), *Entry.Attenuation.ToSoftObjectPath().ToString());
	return nullptr;
}

USoundConcurrency* UNKMSoundManager::LoadConcurrency(const FNKMSoundEntry& Entry) const
{
	if (Entry.Concurrency.IsNull()) return nullptr;
	if (USoundConcurrency* LoadedConcurrency = Entry.Concurrency.Get()) return LoadedConcurrency;

#if 0
	UE_LOG(LogTemp, Error,
		TEXT("[NKMSound] 사운드 PDA의 Gameplay 번들로 동시 재생 설정이 로드되지 않아 동기 강제 로드합니다. 사운드태그=%s 에셋=%s"),
		*Entry.SoundTag.ToString(), *Entry.Concurrency.ToSoftObjectPath().ToString());
#endif
	UE_LOG(LogTemp, Error,
		TEXT("[NKMSound] Concurrency asset was not preloaded. SoundTag=%s Asset=%s"),
		*Entry.SoundTag.ToString(), *Entry.Concurrency.ToSoftObjectPath().ToString());
	return nullptr;
}

bool UNKMSoundManager::CanPlaySound(const UObject* WorldContextObject, const FNKMSoundEntry& Entry, const FVector* Location, const AActor* SourceActor) const
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}
	if (ShouldMuteIsolatedSource(Entry, SourceActor)) return false;

	// 1. 같은 태그가 너무 빠르게 반복 재생되는지 검사합니다.
	// 예를 들어 타쿠 30마리가 같은 프레임에 맞아도 피격음이 과하게 겹치지 않게 합니다.
	if (Entry.Cooldown > 0.0f)
	{
		if (const float* LastPlayTime = LastPlayTimeByTag.Find(Entry.SoundTag))
		{
			if (World->GetTimeSeconds() - *LastPlayTime < Entry.Cooldown)
			{
				return false;
			}
		}
	}

	// 2. 같은 태그의 활성 AudioComponent 개수를 세어 동시 재생 수를 제한합니다.
	// Unreal Concurrency 에셋과 별개로, 코드에서 한 번 더 가볍게 컷하는 역할입니다.
	if (Entry.MaxSimultaneous > 0)
	{
		if (const FNKMActiveSoundComponentList* ActiveComponents = ActiveComponentsByTag.Find(Entry.SoundTag))
		{
			int32 PlayingCount = 0;
			for (UAudioComponent* AudioComponent : ActiveComponents->Components)
			{
				if (AudioComponent && AudioComponent->IsPlaying())
				{
					PlayingCount++;
				}
			}

			if (PlayingCount >= Entry.MaxSimultaneous)
			{
				return false;
			}
		}
	}

	// 3. 위치가 있는 사운드는 플레이어와 너무 멀면 재생하지 않습니다.
	// 싱글게임 기준이라 0번 플레이어 컨트롤러의 Pawn을 청취자 기준으로 사용합니다.
	if (Location && Entry.MaxDistance > 0.0f)
	{
		const APlayerController* PlayerController = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
		const APawn* ListenerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		if (ListenerPawn && FVector::DistSquared(ListenerPawn->GetActorLocation(), *Location) > FMath::Square(Entry.MaxDistance))
		{
			return false;
		}
	}

	return true;
}

void UNKMSoundManager::NotifySoundStarted(const UObject* WorldContextObject, const FNKMSoundEntry& Entry, UAudioComponent* AudioComponent, float PitchMultiplier, AActor* SourceActor)
{
	// 쿨다운 계산용으로 이 태그가 마지막으로 재생된 시간을 기록합니다.
	if (UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr)
	{
		LastPlayTimeByTag.Add(Entry.SoundTag, World->GetTimeSeconds());
	}

	// 동시 재생 수 제한을 위해 재생된 AudioComponent를 추적합니다.
	// bAutoDestroy가 true여도 TObjectPtr이 null이 되거나 IsPlaying이 false가 되면 PruneFinishedSounds에서 정리됩니다.
	if (AudioComponent)
	{
		FNKMActiveSoundComponentList& ActiveComponents = ActiveComponentsByTag.FindOrAdd(Entry.SoundTag);
		ActiveComponents.Components.Add(AudioComponent);
		ActiveComponents.PitchMultipliers.Add(AudioComponent, FMath::Max(0.01f, PitchMultiplier));
		ActiveComponents.SourceActors.Add(AudioComponent, SourceActor);
	}
}

void UNKMSoundManager::BeginSourceIsolation(FGameplayTag SoundTagRoot, AActor* InAudibleSource)
{
	EndSourceIsolation();
	if (!SoundTagRoot.IsValid()) return;

	bSourceIsolationActive = true;
	IsolationSoundTagRoot = SoundTagRoot;
	AudibleSource = InAudibleSource;
	PruneFinishedSounds();

	for (TPair<FGameplayTag, FNKMActiveSoundComponentList>& Pair : ActiveComponentsByTag)
	{
		const FNKMSoundEntry* Entry = EntriesByTag.Find(Pair.Key);
		if (!Entry) continue;

		for (UAudioComponent* AudioComponent : Pair.Value.Components)
		{
			const TWeakObjectPtr<AActor>* SourceActor = Pair.Value.SourceActors.Find(AudioComponent);
			if (!AudioComponent || !AudioComponent->IsPlaying() || !ShouldMuteIsolatedSource(*Entry, SourceActor ? SourceActor->Get() : nullptr))
				continue;

			IsolationVolumeMultipliers.Add(AudioComponent, AudioComponent->VolumeMultiplier);
			AudioComponent->SetVolumeMultiplier(0.f);
		}
	}
}

void UNKMSoundManager::EndSourceIsolation()
{
	for (const TPair<TObjectPtr<UAudioComponent>, float>& Pair : IsolationVolumeMultipliers)
	{
		if (IsValid(Pair.Key) && Pair.Key->IsPlaying()) Pair.Key->SetVolumeMultiplier(Pair.Value);
	}

	IsolationVolumeMultipliers.Empty();
	AudibleSource.Reset();
	IsolationSoundTagRoot = FGameplayTag();
	bSourceIsolationActive = false;
}

bool UNKMSoundManager::IsIsolatedSound(const FNKMSoundEntry& Entry) const
{
	return IsolationSoundTagRoot.IsValid() && Entry.SoundTag.MatchesTag(IsolationSoundTagRoot);
}

bool UNKMSoundManager::ShouldMuteIsolatedSource(const FNKMSoundEntry& Entry, const AActor* SourceActor) const
{
	return bSourceIsolationActive && IsIsolatedSound(Entry) && SourceActor != AudibleSource.Get();
}

void UNKMSoundManager::PruneFinishedSounds()
{
	// 재생이 끝난 컴포넌트 참조를 계속 들고 있으면 MaxSimultaneous가 실제보다 높게 계산됩니다.
	// 볼륨 적용 시점이나 필요 시점에 가볍게 정리합니다.
	for (auto It = ActiveComponentsByTag.CreateIterator(); It; ++It)
	{
		for (auto PitchIt = It.Value().PitchMultipliers.CreateIterator(); PitchIt; ++PitchIt)
		{
			if (!PitchIt.Key() || !PitchIt.Key()->IsPlaying()) PitchIt.RemoveCurrent();
		}
		for (auto SourceIt = It.Value().SourceActors.CreateIterator(); SourceIt; ++SourceIt)
		{
			if (!SourceIt.Key() || !SourceIt.Key()->IsPlaying()) SourceIt.RemoveCurrent();
		}

		It.Value().Components.RemoveAll([](const UAudioComponent* AudioComponent)
		{
			return !AudioComponent || !AudioComponent->IsPlaying();
		});

		if (It.Value().Components.IsEmpty())
		{
			It.RemoveCurrent();
		}
	}
}

void UNKMSoundManager::ApplyTimeDilationEffects(float PitchScale, float LowPassCutoffFrequency, bool bEnableLowPassFilter)
{
	PruneFinishedSounds();
	const float SafePitchScale = FMath::Max(0.01f, PitchScale);

	for (TPair<FGameplayTag, FNKMActiveSoundComponentList>& Pair : ActiveComponentsByTag)
	{
		const FNKMSoundEntry* Entry = EntriesByTag.Find(Pair.Key);
		if (!Entry || Entry->Layer == ENKMSoundLayer::UI)
		{
			continue;
		}

		for (UAudioComponent* AudioComponent : Pair.Value.Components)
		{
			if (AudioComponent && AudioComponent->IsPlaying())
			{
				const float* PitchMultiplier = Pair.Value.PitchMultipliers.Find(AudioComponent);
				AudioComponent->SetPitchMultiplier(Entry->Pitch * SafePitchScale * (PitchMultiplier ? *PitchMultiplier : 1.f));

				const bool bIsLowPassLayer =
					Entry->Layer == ENKMSoundLayer::BGM ||
					Entry->Layer == ENKMSoundLayer::Character ||
					Entry->Layer == ENKMSoundLayer::Crowd;
				AudioComponent->SetLowPassFilterEnabled(bIsLowPassLayer && bEnableLowPassFilter);
				if (bIsLowPassLayer)
				{
					AudioComponent->SetLowPassFilterFrequency(LowPassCutoffFrequency);
				}
			}
		}
	}
}

void UNKMSoundManager::FadeOutGameplaySounds(float FadeOutDuration)
{
	PruneFinishedSounds();

	for (TPair<FGameplayTag, FNKMActiveSoundComponentList>& Pair : ActiveComponentsByTag)
	{
		const FNKMSoundEntry* Entry = EntriesByTag.Find(Pair.Key);
		if (!Entry || Entry->Layer == ENKMSoundLayer::UI) continue;

		for (UAudioComponent* AudioComponent : Pair.Value.Components)
		{
			if (AudioComponent && AudioComponent->IsPlaying())
			{
				AudioComponent->FadeOut(FMath::Max(0.0f, FadeOutDuration), 0.0f, EAudioFaderCurve::Sin);
			}
		}
	}
}

void UNKMSoundManager::ApplyGameplayLowPass(float LowPassCutoffFrequency, bool bEnableLowPassFilter)
{
	PruneFinishedSounds();

	for (TPair<FGameplayTag, FNKMActiveSoundComponentList>& Pair : ActiveComponentsByTag)
	{
		const FNKMSoundEntry* Entry = EntriesByTag.Find(Pair.Key);
		if (!Entry || Entry->Layer == ENKMSoundLayer::UI) continue;

		for (UAudioComponent* AudioComponent : Pair.Value.Components)
		{
			if (AudioComponent && AudioComponent->IsPlaying())
			{
				AudioComponent->SetLowPassFilterEnabled(bEnableLowPassFilter);
				AudioComponent->SetLowPassFilterFrequency(LowPassCutoffFrequency);
			}
		}
	}
}
