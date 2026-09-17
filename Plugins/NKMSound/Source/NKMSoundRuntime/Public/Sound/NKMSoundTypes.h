#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"
#include "NKMSoundTypes.generated.h"

// 사운드를 어떤 볼륨 그룹으로 취급할지 구분하는 값입니다.
// 실제 Unreal SoundClass를 직접 조작하기 전 단계에서 코드 레벨 볼륨 계산에 사용합니다.
UENUM(BlueprintType)
enum class ENKMSoundLayer : uint8
{
	// 총소리, 피격음, 폭발음 같은 일반 게임플레이 효과음입니다.
	SFX,

	// 배경 음악입니다. PlayBGM/StopBGM에서 주로 사용합니다.
	BGM,

	// 타쿠 무리 수에 따라 커지거나 작아지는 군중 루프 사운드입니다.
	Crowd,

	// 버튼 클릭, 메뉴 열림 같은 2D UI 사운드입니다.
	UI,

	// 특수 타쿠 등장 대사나 캐릭터 보이스처럼 따로 볼륨을 빼고 싶을 때 사용합니다.
	Voice,

	// 타쿠의 공격음, 발소리, 보이스, 이동 루프 등 산데비스탄 LPF 대상 사운드입니다.
	// 기존 직렬화된 enum 값을 보존하기 위해 항상 마지막에 추가합니다.
	Character
};

// 데이터 에셋에 등록하는 "사운드 재생 규칙"입니다.
// 코드는 SoundTag만 요청하고, 실제 어떤 USoundBase를 어떻게 재생할지는 이 구조체가 결정합니다.
USTRUCT(BlueprintType)
struct NKMSOUNDRUNTIME_API FNKMSoundEntry
{
	GENERATED_BODY()

	// 코드에서 요청할 식별자입니다.
	// 예: Game.Sound.Weapon.Rifle.Fire, Game.Sound.Character.Enemy.Spawn
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	FGameplayTag SoundTag;

	// 실제 재생할 사운드 에셋입니다.
	// SoundWave, SoundCue, MetaSound Source/Preset 등 USoundBase 계열을 넣을 수 있습니다.
	// SoftObjectPtr로 둔 이유는 데이터 에셋을 참조하더라도 즉시 전부 로드하지 않기 위해서입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (AssetBundles = "Gameplay"))
	TSoftObjectPtr<USoundBase> Sound;

	// 이 사운드가 어떤 볼륨 레이어에 속하는지 정합니다.
	// Subsystem에서 Master/BGM/SFX/Crowd 볼륨 배율 계산에 사용합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	ENKMSoundLayer Layer = ENKMSoundLayer::SFX;

	// 이 사운드 자체의 기본 볼륨입니다.
	// 최종 볼륨 = Entry.Volume * 레이어 볼륨 * MasterVolume 형태로 계산됩니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.0"))
	float Volume = 1.0f;

	// 이 사운드 자체의 기본 피치입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.1"))
	float Pitch = 1.0f;

	// 3D 사운드 감쇠 설정입니다.
	// 비워두면 해당 사운드 에셋 또는 엔진 기본 설정을 따릅니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (AssetBundles = "Gameplay"))
	TSoftObjectPtr<USoundAttenuation> Attenuation;

	// Unreal 오디오 엔진의 동시 재생 제한 설정입니다.
	// 코드 레벨 MaxSimultaneous와 별개로, 엔진 차원에서 Stop Farthest 같은 정책을 적용할 수 있습니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (AssetBundles = "Gameplay"))
	TSoftObjectPtr<USoundConcurrency> Concurrency;

	// 같은 SoundTag가 너무 자주 재생되지 않게 막는 최소 간격입니다.
	// 예: 피격음 Cooldown 0.1이면 0.1초 안에 같은 태그가 반복 요청되어도 무시합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limit", meta = (ClampMin = "0.0"))
	float Cooldown = 0.0f;

	// 플레이어와 이 거리보다 멀면 재생하지 않습니다.
	// 0이면 거리 제한을 사용하지 않습니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limit", meta = (ClampMin = "0.0"))
	float MaxDistance = 0.0f;

	// 같은 SoundTag가 동시에 몇 개까지 들릴 수 있는지 코드 레벨에서 제한합니다.
	// 0이면 이 제한을 사용하지 않습니다. 타쿠 피격음처럼 같은 소리가 많이 겹치는 곳에 유용합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limit", meta = (ClampMin = "0"))
	int32 MaxSimultaneous = 0;
};
