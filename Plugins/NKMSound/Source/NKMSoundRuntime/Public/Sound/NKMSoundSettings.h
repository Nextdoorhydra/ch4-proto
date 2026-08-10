#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/AssetManagerTypes.h"
#include "GameplayTagContainer.h"
#include "NKMSoundSettings.generated.h"

// Project Settings에 노출되는 NetKarma 사운드 설정입니다.
// UIManagerSettings처럼 "어떤 데이터 에셋을 전역으로 사용할지"를 프로젝트 설정에서 지정합니다.
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "NKM Sound"))
class NKMSOUNDRUNTIME_API UNKMSoundSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// PDA 로더를 통해 로드할 사운드 주 데이터 에셋 목록입니다.
	UPROPERTY(Config, EditAnywhere, Category = "Sound|Primary Assets", meta = (AllowedTypes = "NKMSoundDataAsset"))
	TArray<FPrimaryAssetId> SoundDataAssetIds;

	// 재생 요청 시 사운드 태그에 대응하는 PDA가 아직 캐시되지 않았을 때 사용할 방어 로드 매핑입니다.
	// 가장 구체적으로 일치하는 루트 태그의 에셋 ID를 선택합니다.
	UPROPERTY(Config, EditAnywhere, Category = "Sound|Primary Assets", meta = (AllowedTypes = "NKMSoundDataAsset"))
	TMap<FGameplayTag, FPrimaryAssetId> SoundDataAssetIdByTagRoot;

	// 개별 Entry.MaxDistance가 0일 때 기본 거리 제한으로 확장할 수 있도록 남겨둔 값입니다.
	// 현재 코드는 Entry.MaxDistance가 0이면 거리 제한을 쓰지 않지만,
	// 프로젝트 정책이 정해지면 이 값을 기본 거리로 쓰게 바꾸면 됩니다.
	UPROPERTY(Config, EditAnywhere, Category = "Sound", meta = (ClampMin = "0.0"))
	float DefaultRelevantDistance = 3000.0f;

	// BGM을 교체하거나 정비 시간으로 전환할 때 FadeIn/FadeOut에 함께 사용하는 시간입니다.
	UPROPERTY(Config, EditAnywhere, Category = "Sound", meta = (ClampMin = "0.0"))
	float BGMCrossfadeTime = 1.0f;

	// 군중 루프 사운드를 바꿀 때 기존 루프를 FadeOut하는 기본 시간입니다.
	UPROPERTY(Config, EditAnywhere, Category = "Sound", meta = (ClampMin = "0.0"))
	float CrowdFadeTime = 0.5f;

	// 슬로 모션 진입 시 전체 사운드 피치가 목표값까지 내려가는 시간입니다.
	UPROPERTY(Config, EditAnywhere, Category = "Sound|Time Dilation", meta = (ClampMin = "0.0"))
	float SlowMotionPitchFadeInTime = 0.5f;

	// 슬로 모션 종료 시 전체 사운드 피치가 원래 값으로 돌아오는 시간입니다.
	UPROPERTY(Config, EditAnywhere, Category = "Sound|Time Dilation", meta = (ClampMin = "0.0"))
	float SlowMotionPitchFadeOutTime = 0.5f;

	// 지나치게 낮은 피치로 소리가 뭉개지는 것을 막는 하한값입니다.
	UPROPERTY(Config, EditAnywhere, Category = "Sound|Time Dilation", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float MinimumTimeDilationPitch = 0.70f;

	// 슬로 모션 활성화 중 BGM에만 추가로 곱할 볼륨 배율입니다.
	UPROPERTY(Config, EditAnywhere, Category = "Sound|Time Dilation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SlowMotionBGMVolumeMultiplier = 0.35f;

	// 시간 배율 효과 중 BGM, Character, Crowd 레이어에 적용할 Low Pass Filter Cutoff입니다.
	UPROPERTY(Config, EditAnywhere, Category = "Sound|Time Dilation", meta = (ClampMin = "20.0", ClampMax = "20000.0"))
	float SlowMotionLowPassCutoffFrequency = 1500.0f;

	// 정상 상태의 Cutoff입니다. 전환이 끝나면 LPF 자체도 비활성화됩니다.
	UPROPERTY(Config, EditAnywhere, Category = "Sound|Time Dilation", meta = (ClampMin = "20.0", ClampMax = "20000.0"))
	float NormalLowPassCutoffFrequency = 20000.0f;

};
