#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "PrimaryDataAssetBase.h"
#include "Stage/Obstacle/Component/CMHazardComponent.h"

#include "CMObstacleDefinition.generated.h"

class UGameplayEffect;
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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Gameplay",
        meta = (AssetBundles = "Gameplay"))
    TSoftClassPtr<UGameplayEffect> GameplayEffectClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Gameplay")
    FGameplayTag HazardEffectTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Gameplay")
    ECMHazardApplicationMode ApplicationMode = ECMHazardApplicationMode::Single;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Gameplay",
        meta = (ClampMin = "0.01"))
    float PeriodSeconds = 1.0f;
};
