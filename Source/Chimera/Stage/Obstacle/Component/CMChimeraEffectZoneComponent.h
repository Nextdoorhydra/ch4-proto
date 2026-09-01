#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "Stage/Obstacle/Data/CMObstacleEffectTypes.h"

#include "CMChimeraEffectZoneComponent.generated.h"

class ACMChimera;
class UGameplayEffect;

UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
// 키메라 전체에 적용되는 장애물 GE를 공용 ASC에 적용하고 제거
class CHIMERA_API UCMChimeraEffectZoneComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMChimeraEffectZoneComponent();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Obstacle|Chimera Effect")
    void NotifyTargetEntered(AActor* TargetActor);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
        Category = "Chimera|Obstacle|Chimera Effect")
    void NotifyTargetExited(AActor* TargetActor);

    UFUNCTION(BlueprintCallable,
        Category = "Chimera|Obstacle|Chimera Effect")
    void SetZoneEnabled(bool bEnabled);

    // 장애물에 직접 설정된 GE 클래스와 적용 정책 저장
    void ConfigureChimeraEffect(
        const FCMChimeraObstacleEffectConfig& NewConfig,
        TSubclassOf<UGameplayEffect> NewGameplayEffectClass);

private:
    struct FTrackedChimera
    {
        int32 OverlapCount = 0;
        FActiveGameplayEffectHandle PersistentEffectHandle;
    };

    bool ApplyEffect(
        ACMChimera& Chimera,
        FActiveGameplayEffectHandle* OutHandle = nullptr) const;
    void RemovePersistentEffect(
        ACMChimera& Chimera,
        FTrackedChimera& Tracked) const;
    void UpdatePeriodicTimer();
    void HandlePeriodicApplication();

    bool bZoneEnabled = true;
    FCMChimeraObstacleEffectConfig ChimeraEffect;
    TSubclassOf<UGameplayEffect> GameplayEffectClass;
    TMap<TWeakObjectPtr<ACMChimera>, FTrackedChimera> TrackedChimeras;
    FTimerHandle PeriodicTimerHandle;
};

namespace CMObstacleEffectDataNames
{
    CHIMERA_API extern const FName Damage;
    CHIMERA_API extern const FName Duration;
    CHIMERA_API extern const FName PrimaryStatusValue;
    CHIMERA_API extern const FName SecondaryStatusValue;
}
