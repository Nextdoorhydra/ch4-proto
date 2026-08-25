#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Stage/Obstacle/Data/CMObstacleEffectTypes.h"

#include "CMHazardComponent.generated.h"

class ACMPartActorBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCMHazardTargetSignature, AActor*, TargetActor);

UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
// 접촉한 팔과 다리에 코드 기반 내구도 피해 및 런타임 상태 적용
class CHIMERA_API UCMHazardComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMHazardComponent();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Hazard")
    void NotifyTargetEntered(AActor* TargetActor);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Hazard")
    void NotifyTargetExited(AActor* TargetActor);

    UFUNCTION(BlueprintCallable, Category = "Chimera|Hazard")
    void SetHazardEnabled(bool bEnabled);

    // Definition PDA와 함께 준비된 값 기반 파츠 효과 설정
    void ConfigurePartEffect(const FCMPartObstacleEffectConfig& NewConfig);

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Hazard")
    FCMHazardTargetSignature OnTargetEntered;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Hazard")
    FCMHazardTargetSignature OnTargetExited;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Chimera|Hazard")
    FCMPartObstacleEffectConfig PartEffect;

private:
    struct FTrackedPart
    {
        int32 OverlapCount = 0;
    };

    ACMPartActorBase* ResolveSupportedPart(AActor* TargetActor) const;
    void ApplyConfiguredEffect(ACMPartActorBase& PartActor);
    void UpdatePeriodicTimer();
    void HandlePeriodicApplication();

    bool bHazardEnabled = true;
    TMap<TWeakObjectPtr<ACMPartActorBase>, FTrackedPart> TrackedParts;
    FTimerHandle PeriodicTimerHandle;
};
