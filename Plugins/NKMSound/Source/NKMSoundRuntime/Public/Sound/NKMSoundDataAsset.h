#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Sound/NKMSoundCatalogProvider.h"
#include "NKMSoundTypes.h"
#include "NKMSoundDataAsset.generated.h"

// NetKarma 사운드 데이터베이스 역할을 하는 데이터 에셋입니다.
// 디자이너/프로그래머가 에디터에서 이 에셋을 만들고 SoundEntries에 사운드를 등록합니다.
//
// 사용 흐름:
// 1. 에디터에서 UNKMSoundDataAsset 생성
// 2. SoundEntries에 SoundTag와 실제 사운드 에셋 등록
// 3. Project Settings > NKM Sound > Sound Data Asset Ids에 이 에셋 추가
// 4. Stage Data Asset에서 로드 타이밍 지정
// 5. 코드에서는 사운드 태그만 넘겨서 재생 요청
UCLASS(BlueprintType)
class NKMSOUNDRUNTIME_API UNKMSoundDataAsset : public UPrimaryDataAsset, public INKMSoundCatalogProvider
{
	GENERATED_BODY()

public:
	virtual const TArray<FNKMSoundEntry>& GetSoundEntries() const override { return SoundEntries; }
	virtual FPrimaryAssetId GetSoundCatalogId() const override { return GetPrimaryAssetId(); }

	// 사운드 태그별 재생 정보 목록입니다.
	// 같은 사운드 태그가 여러 데이터 에셋에 중복 등록되면 오류를 기록하고 먼저 등록된 항목을 유지합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TArray<FNKMSoundEntry> SoundEntries;
};
