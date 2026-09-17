#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "Sound/NKMSoundTypes.h"
#include "NKMSoundManager.generated.h"

class UAudioComponent;
class AActor;

// UHT는 UPROPERTY TMap의 Value 타입으로 TArray를 직접 쓰는 것을 허용하지 않습니다.
// 그래서 SoundTag별 AudioComponent 배열을 구조체로 한 번 감싸서 보관합니다.
USTRUCT()
struct FNKMActiveSoundComponentList
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<UAudioComponent>> Components;

	UPROPERTY()
	TMap<TObjectPtr<UAudioComponent>, float> PitchMultipliers;

	UPROPERTY()
	TMap<TObjectPtr<UAudioComponent>, TWeakObjectPtr<AActor>> SourceActors;
};

// 사운드 데이터 검색과 재생 제한 정책을 담당하는 내부 매니저입니다.
//
// 중요한 점:
// - 게임 코드가 직접 이 클래스를 호출하지 않는 것을 기본으로 합니다.
// - 외부에서는 UNKMSoundSubsystem을 호출하고, Subsystem이 이 Manager에게 일을 맡깁니다.
// - UObject로 만든 이유는 GC가 AudioComponent 참조를 추적할 수 있게 하기 위해서입니다.
UCLASS()
class NKMSOUNDRUNTIME_API UNKMSoundManager : public UObject
{
	GENERATED_BODY()

public:
	// PDA 로더에서 전달받은 사운드 데이터 에셋들로 사운드 태그 조회 맵을 만듭니다.
	void Initialize(const TArray<UObject*>& SoundCatalogs);

	// 맵 전환/게임 종료 시 런타임 상태를 비웁니다.
	void ResetRuntimeState();

	// SoundTag에 해당하는 재생 규칙을 찾습니다.
	const FNKMSoundEntry* FindEntry(FGameplayTag SoundTag) const;

	// SoftObjectPtr로 등록된 실제 사운드 에셋을 동기 로드합니다.
	USoundBase* LoadSound(const FNKMSoundEntry& Entry) const;

	// 3D 감쇠 설정을 동기 로드합니다.
	USoundAttenuation* LoadAttenuation(const FNKMSoundEntry& Entry) const;

	// Unreal Concurrency 설정을 동기 로드합니다.
	USoundConcurrency* LoadConcurrency(const FNKMSoundEntry& Entry) const;

	// 이 사운드를 지금 재생해도 되는지 검사합니다.
	// 쿨다운, 동시 재생 수, 플레이어와의 거리 제한을 여기서 처리합니다.
	bool CanPlaySound(const UObject* WorldContextObject, const FNKMSoundEntry& Entry, const FVector* Location = nullptr, const AActor* SourceActor = nullptr) const;

	// 실제 재생이 시작된 뒤 호출되어 최근 재생 시간과 활성 AudioComponent를 기록합니다.
	void NotifySoundStarted(const UObject* WorldContextObject, const FNKMSoundEntry& Entry, UAudioComponent* AudioComponent, float PitchMultiplier = 1.0f, AActor* SourceActor = nullptr);

	void BeginSourceIsolation(FGameplayTag SoundTagRoot, AActor* AudibleSource);
	void EndSourceIsolation();

	// 이미 끝난 AudioComponent 참조를 정리합니다.
	// MaxSimultaneous 계산이 죽은 컴포넌트 때문에 막히지 않게 합니다.
	void PruneFinishedSounds();

	// 활성 게임 사운드에 시간 배율 피치를 반영합니다. UI 레이어는 명시적으로 제외합니다.
	void ApplyTimeDilationEffects(float PitchScale, float LowPassCutoffFrequency, bool bEnableLowPassFilter);

	void FadeOutGameplaySounds(float FadeOutDuration);
	void ApplyGameplayLowPass(float LowPassCutoffFrequency, bool bEnableLowPassFilter);

	// 현재 재생 중인 게임 사운드의 Pitch를 월드 시간 배율에 맞춰 갱신한다. UI 레이어는 제외한다.

private:
	// 데이터 에셋에서 모은 SoundTag -> Entry 조회 테이블입니다.
	UPROPERTY()
	TMap<FGameplayTag, FNKMSoundEntry> EntriesByTag;

	// SoundTag별 마지막 재생 시각입니다. Cooldown 체크에 사용합니다.
	UPROPERTY()
	TMap<FGameplayTag, float> LastPlayTimeByTag;

	// SoundTag별 현재 재생 중인 AudioComponent 목록입니다. MaxSimultaneous 체크에 사용합니다.
	UPROPERTY()
	TMap<FGameplayTag, FNKMActiveSoundComponentList> ActiveComponentsByTag;

	UPROPERTY()
	TMap<TObjectPtr<UAudioComponent>, float> IsolationVolumeMultipliers;

	TWeakObjectPtr<AActor> AudibleSource;
	FGameplayTag IsolationSoundTagRoot;
	bool bSourceIsolationActive = false;
	bool IsIsolatedSound(const FNKMSoundEntry& Entry) const;
	bool ShouldMuteIsolatedSource(const FNKMSoundEntry& Entry, const AActor* SourceActor) const;
};
