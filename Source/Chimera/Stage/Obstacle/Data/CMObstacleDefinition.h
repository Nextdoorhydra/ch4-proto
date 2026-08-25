#pragma once

#include "CoreMinimal.h"
#include "PrimaryDataAssetBase.h"
#include "Stage/Obstacle/Data/CMObstacleEffectTypes.h"

#include "CMObstacleDefinition.generated.h"

class UMaterialInterface;
class UNiagaraSystem;
class USoundBase;
class UStaticMesh;

UCLASS(BlueprintType)
// 장애물의 공용 외형 에셋과 위험 효과 설정을 비동기 로드 단위로 묶음
class CHIMERA_API UCMObstacleDefinition : public UPrimaryDataAssetBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Visual",
        meta = (AssetBundles = "Gameplay"))
    TSoftObjectPtr<UStaticMesh> PrimaryMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Visual",
        meta = (AssetBundles = "Gameplay"))
    TArray<TSoftObjectPtr<UMaterialInterface>> Materials;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Effect",
        meta = (AssetBundles = "Gameplay"))
    TSoftObjectPtr<UNiagaraSystem> NiagaraSystem;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Audio",
        meta = (AssetBundles = "Gameplay"))
    TSoftObjectPtr<USoundBase> LoopSound;

    // 팔과 다리처럼 개별 파츠에 코드로 적용할 내구도 및 상태 설정
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Obstacle|Part Effect")
    FCMPartObstacleEffectConfig PartEffect;

    // 키메라 전체의 공용 ASC에 적용할 GameplayEffect 설정
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Obstacle|Chimera Effect")
    FCMChimeraObstacleEffectConfig ChimeraEffect;
};
