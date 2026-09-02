#pragma once

#include "CoreMinimal.h"
#include "PrimaryDataAssetBase.h"
#include "Stage/Obstacle/Data/CMObstacleEffectTypes.h"

#include "CMObstacleDefinition.generated.h"

class UMaterialInterface;
class UNiagaraSystem;
class USoundBase;
class UStaticMesh;

UCLASS(Abstract, BlueprintType)
// 기존 장애물 Definition 에셋의 호환 로딩만 유지하며 신규 장애물에는 사용하지 않음
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

};
