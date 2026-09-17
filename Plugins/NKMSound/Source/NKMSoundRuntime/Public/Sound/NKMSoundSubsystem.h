#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "Containers/Ticker.h"
#include "NKMSoundTypes.h"
#include "NKMSoundSubsystem.generated.h"

class UAudioComponent;
class AActor;
class USceneComponent;
class USoundBase;
class USoundConcurrency;
class UNKMSoundDataAsset;
class UNKMSoundManager;
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FNKMSoundVolumeSaveRequested,
	float,
	float,
	float);

// NetKarma의 전역 사운드 호출 창구입니다.
//
// 싱글게임 기준 설계:
// - 총, 수류탄, 타쿠, UI, BGM 등 모든 코드는 이 Subsystem을 경유합니다.
// - 실제 데이터 조회와 제한 정책은 UNKMSoundManager에게 위임합니다.
UCLASS()
class NKMSOUNDRUNTIME_API UNKMSoundSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// GameInstance 생성 시 자동 호출됩니다.
	// Manager를 만들고 PDALoader의 완료 메시지를 구독합니다.
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// GameInstance 종료 시 자동 호출됩니다.
	// BGM/군중 루프를 멈추고 런타임 상태를 정리합니다.
	virtual void Deinitialize() override;

	// PDALoader가 이미 캐시한 Sound PDA들로 로컬 조회 테이블을 다시 구성합니다.
	// 이 함수 자체는 에셋을 로드하지 않습니다.
	UFUNCTION(BlueprintCallable, Category = "NKM|Sound")
	void ReloadSoundData();

	// The host project owns asynchronous loading and registers loaded catalogs here.
	UFUNCTION(BlueprintCallable, Category = "NKM|Sound|Data")
	bool RegisterSoundDataAsset(UNKMSoundDataAsset* SoundDataAsset);

	UFUNCTION(BlueprintCallable, Category = "NKM|Sound|Data")
	bool RegisterSoundCatalog(UObject* SoundCatalog);

	UFUNCTION(BlueprintCallable, Category = "NKM|Sound|Data")
	void ClearSoundDataAssets();

	FNKMSoundVolumeSaveRequested& OnVolumeSaveRequested()
	{
		return VolumeSaveRequested;
	}

	// 월드 위치에서 3D 효과음을 재생합니다.
	// 총소리, 폭발음, 피격음처럼 위치감이 필요한 소리에 사용합니다.
	UFUNCTION(BlueprintCallable, Category = "NKM|Sound")
	UAudioComponent* PlaySFX(FGameplayTag SoundTag, FVector Location);
	UAudioComponent* PlaySFXForActor(FGameplayTag SoundTag, FVector Location, AActor* SourceActor);

	UFUNCTION(BlueprintCallable, Category = "NKM|Sound")
	UAudioComponent* PlaySFX2D(FGameplayTag SoundTag);

	// 기존 PlaySFX와 동일하지만 이번 재생에만 추가 볼륨 배율을 적용한다.
	// 기존 BP PlaySFX 노드의 핀 구성을 바꾸지 않기 위해 별도 함수로 제공한다.
	UFUNCTION(BlueprintCallable, Category = "NKM|Sound")
	UAudioComponent* PlaySFXWithVolume(FGameplayTag SoundTag, FVector Location, float VolumeMultiplier = 1.0f);
	UAudioComponent* PlaySFXWithPitch(FGameplayTag SoundTag, FVector Location, float PitchMultiplier);

	// 특정 컴포넌트/소켓에 붙어서 따라다니는 3D 효과음을 재생합니다.
	// 무기 총구, 캐릭터 몸, 이동 중인 액터에 붙는 소리에 사용합니다.
	UFUNCTION(BlueprintCallable, Category = "NKM|Sound")
	UAudioComponent* PlayAttachedSFX(FGameplayTag SoundTag, USceneComponent* AttachToComponent, FName SocketName = NAME_None);
	UAudioComponent* PlayAttachedSFXWithPitch(FGameplayTag SoundTag, USceneComponent* AttachToComponent, float PitchMultiplier, FName SocketName = NAME_None);

	// 2D UI 사운드를 재생합니다.
	// 위치감이 없고 화면 전체에서 들리는 버튼/메뉴 사운드에 사용합니다.
	UFUNCTION(BlueprintCallable, Category = "NKM|Sound")
	UAudioComponent* PlayUISound(FGameplayTag SoundTag);

	// BGM을 재생합니다.
	// 이미 같은 BGM이 재생 중이면 bRestartIfSame이 true일 때만 다시 시작합니다.
	UFUNCTION(BlueprintCallable, Category = "NKM|Sound")
	void PlayBGM(FGameplayTag SoundTag, bool bRestartIfSame = false);

	// 웨이브 전투 BGM을 재생한다. 정비 시간에 보관한 같은 곡이 있으면 멈춘 위치부터 재개한다.
	void PlayBattleBGM(FGameplayTag SoundTag);

	// 현재 BGM을 일시정지해 보관하고 정비 시간 BGM으로 전환한다.
	void PlayIntermissionBGM(FGameplayTag SoundTag);

	// 현재 BGM을 FadeOut 후 정지합니다.
	UFUNCTION(BlueprintCallable, Category = "NKM|Sound")
	void StopBGM(float FadeOutTime = 1.0f);

	void SetBGMGamePaused(bool bPaused);

	// 군중 루프 사운드를 재생합니다.
	// 타쿠 수에 따라 볼륨을 조절할 루프 사운드 1개를 유지하는 용도입니다.
	UFUNCTION(BlueprintCallable, Category = "NKM|Sound")
	void PlayCrowdLoop(FGameplayTag SoundTag);

	// 현재 군중 루프 사운드를 FadeOut 후 정지합니다.
	UFUNCTION(BlueprintCallable, Category = "NKM|Sound")
	void StopCrowdLoop(float FadeOutTime = 0.5f);

	// 지속 Crowd 레이어의 정규화된 볼륨 강도를 갱신합니다.
	UFUNCTION(BlueprintCallable, Category = "NKM|Sound")
	void SetCrowdIntensity(float Intensity);

	// 전체 볼륨입니다. 모든 레이어에 곱해집니다.
	UFUNCTION(BlueprintCallable, Category = "NKM|Sound|Volume")
	void SetMasterVolume(float Volume);

	// BGM 레이어 볼륨입니다.
	UFUNCTION(BlueprintCallable, Category = "NKM|Sound|Volume")
	void SetBGMVolume(float Volume);

	// 일반 효과음 레이어 볼륨입니다.
	UFUNCTION(BlueprintCallable, Category = "NKM|Sound|Volume")
	void SetSFXVolume(float Volume);

	// 군중 루프 레이어 볼륨입니다.
	// 이벤트성 Crowd 사운드는 MasterVolume과 CrowdVolume만 사용합니다.
	// CrowdIntensity는 기존 지속 군중 루프에만 적용됩니다.
	UFUNCTION(BlueprintCallable, Category = "NKM|Sound|Volume")
	void SetCrowdVolume(float Volume);

	// 메모리에 변경된 Master/BGM/SFX 볼륨을 GameUserSettings.ini에 기록합니다.
	// 슬라이더 이동마다 호출하지 말고 옵션 화면을 닫는 시점에 한 번 호출합니다.
	UFUNCTION(BlueprintCallable, Category = "NKM|Sound|Volume")
	void SaveVolumeSettings();

	// World Time Dilation 변경 직후 현재 재생 중인 BGM/SFX/Crowd/Voice Pitch를 갱신한다. UI는 제외한다.
	UFUNCTION(BlueprintCallable, Category = "NKM|Sound")
	void ApplyTimeDilationAudioTransition();

	UFUNCTION(BlueprintCallable, Category = "NKM|Sound")
	void BeginGameplayAudioFade(float FadeDuration = 8.f, float LowPassCutoffFrequency = 1000.0f);

	void ResetGameplayAudioFade();
	void ResetRuntimeMixState();
	void BeginDucking(float Duration, float VolumeMultiplier);
	void BeginSourceIsolation(FGameplayTag SoundTagRoot, AActor* AudibleSource);
	void EndSourceIsolation();

	UFUNCTION(BlueprintPure, Category = "NKM|Sound|Volume")
	float GetMasterVolume() const { return MasterVolume; }

	UFUNCTION(BlueprintPure, Category = "NKM|Sound|Volume")
	float GetBGMVolume() const { return BGMVolume; }

	UFUNCTION(BlueprintPure, Category = "NKM|Sound|Volume")
	float GetSFXVolume() const { return SFXVolume; }

	UFUNCTION(BlueprintPure, Category = "NKM|Sound|Volume")
	float GetCrowdVolume() const { return CrowdVolume; }

private:
	void ValidateSoundDataConfiguration() const;
	void ValidateSoundDataAssetRoutes(const UObject* SoundCatalog) const;
	void RebuildSoundManagerEntries();
	const FNKMSoundEntry* FindOrLoadEntry(FGameplayTag SoundTag);
	FPrimaryAssetId ResolveSoundDataAssetIdForTag(FGameplayTag SoundTag) const;

	float GetBGMTransitionTime() const;
	bool StartBGM(FGameplayTag SoundTag, float FadeInTime);
	bool ResumeSuspendedBGM(FGameplayTag SoundTag, float FadeInTime);
	void SuspendActiveBGM(float FadeOutTime);
	void PauseSuspendedBGM();
	void ClearBGMSuspendTimer();
	void StopActiveBGM(float FadeOutTime);
	void ClearSuspendedBGM();

	// 2D 사운드 재생 공통 함수입니다. UI/BGM/Crowd 루프에서 사용합니다.
	UAudioComponent* SpawnSound2DFromEntry(const FNKMSoundEntry& Entry, bool bAutoDestroy = true, float FadeInTime = 0.f);

	// 월드 위치 기반 3D 사운드 재생 공통 함수입니다.
	UAudioComponent* SpawnSoundAtLocationFromEntry(const FNKMSoundEntry& Entry, const FVector& Location, float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f, AActor* SourceActor = nullptr);

	// 컴포넌트 부착형 3D 사운드 재생 공통 함수입니다.
	UAudioComponent* SpawnSoundAttachedFromEntry(const FNKMSoundEntry& Entry, USceneComponent* AttachToComponent, FName SocketName, float PitchMultiplier = 1.0f);

	// Entry.Layer에 맞는 현재 볼륨 배율을 계산합니다.
	float GetLayerVolume(ENKMSoundLayer Layer) const;

	// 이미 재생 중인 BGM/Crowd AudioComponent에 최신 볼륨 값을 반영합니다.
	void ApplyPersistentVolumes();

	// 페이드 중인 현재 피치 배율을 Entry에 적용합니다. UI는 항상 원본 피치를 사용합니다.
	float GetEntryPitch(const FNKMSoundEntry& Entry) const;
	void ApplyEntryTimeDilationEffects(UAudioComponent* AudioComponent, const FNKMSoundEntry& Entry, float PitchMultiplier = 1.0f) const;
	bool TickTimeDilationAudioTransition(float DeltaTime);
	void StopTimeDilationAudioTicker();
	bool TickGameplayAudioFade(float DeltaTime);
	void StopGameplayAudioFadeTicker();
	void EndDucking();

	// 유지형 사운드(BGM/Crowd)의 Entry.Volume을 저장해둡니다.
	// AudioComponent::SetVolumeMultiplier는 값을 덮어쓰기 때문에, 옵션 볼륨만 갱신하면 Entry.Volume이 사라질 수 있습니다.
	float CurrentBGMEntryVolume = 1.0f;
	float SuspendedBGMEntryVolume = 1.0f;

	float CurrentCrowdEntryVolume = 1.0f;

	// 데이터 조회/재생 제한 정책을 담당하는 내부 매니저입니다.
	UPROPERTY()
	TObjectPtr<UNKMSoundManager> SoundManager;

	UPROPERTY(Transient)
	TObjectPtr<USoundConcurrency> DedicatedBGMConcurrency;

	FNKMSoundVolumeSaveRequested VolumeSaveRequested;

	// 시간 배율 피치 전환 상태입니다. CoreTicker를 사용해 월드 슬로모션과 무관한 실제 시간으로 보간합니다.
	FTSTicker::FDelegateHandle TimeDilationAudioTickerHandle;
	float CurrentTimeDilationPitch = 1.0f;
	float PitchTransitionStart = 1.0f;
	float PitchTransitionTarget = 1.0f;
	float PitchTransitionElapsed = 0.0f;
	float PitchTransitionDuration = 0.0f;
	float CurrentTimeDilationBGMVolume = 1.0f;
	float BGMVolumeTransitionStart = 1.0f;
	float BGMVolumeTransitionTarget = 1.0f;
	float CurrentLowPassCutoffFrequency = 20000.0f;
	float LowPassTransitionStart = 20000.0f;
	float LowPassTransitionTarget = 20000.0f;
	float NormalLowPassCutoffFrequency = 20000.0f;

	FTSTicker::FDelegateHandle GameplayAudioFadeTickerHandle;
	float GameplayAudioFadeElapsed = 0.0f;
	float GameplayAudioFadeDuration = 0.0f;
	float GameplayLowPassStart = 20000.0f;
	float GameplayLowPassTarget = 1000.0f;
	bool bGameplayAudioFadeActive = false;

	FTimerHandle DuckingTimerHandle;
	float DuckingVolumeMultiplier = 1.0f;

	// PDALoader에서 전달받아 등록한 사운드 데이터 에셋입니다.
	UPROPERTY()
	TArray<TObjectPtr<UObject>> LoadedSoundCatalogs;

	// 현재 재생 중인 BGM AudioComponent입니다.
	UPROPERTY()
	TObjectPtr<UAudioComponent> BGMAudioComponent;

	// 정비 시간 동안 재생 위치를 유지한 채 일시정지해두는 배틀 BGM이다.
	UPROPERTY()
	TObjectPtr<UAudioComponent> SuspendedBGMAudioComponent;

	// 전투 BGM의 FadeOut이 끝난 뒤 재생 위치를 보존한 채 Pause하기 위한 타이머입니다.
	FTimerHandle BGMSuspendTimerHandle;

	// 현재 재생 중인 군중 루프 AudioComponent입니다.
	UPROPERTY()
	TObjectPtr<UAudioComponent> CrowdAudioComponent;

	// 현재 BGM 태그입니다. 같은 BGM 중복 재생 방지에 사용합니다.
	UPROPERTY()
	FGameplayTag CurrentBGMTag;

	FGameplayTag SuspendedBGMTag;
	bool bBGMGamePaused = false;

	// 현재 군중 루프 태그입니다. 같은 루프 중복 재생 방지에 사용합니다.
	UPROPERTY()
	FGameplayTag CurrentCrowdTag;

	// 전체 볼륨입니다.
	float MasterVolume = 1.0f;

	// BGM 볼륨입니다.
	float BGMVolume = 1.0f;

	// 일반 효과음 볼륨입니다.
	float SFXVolume = 1.0f;

	// 군중 루프 기본 볼륨입니다.
	float CrowdVolume = 1.0f;

	// 타쿠 수 기반 군중 강도입니다. 0이면 군중 소리가 안 들리고, 1이면 CrowdVolume 그대로 들립니다.
	float CrowdIntensity = 0.0f;
};
