#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Stage/Obstacle/Data/CMObstacleEffectTypes.h"

#include "CMHazardComponent.generated.h"

class ACMPartActorBase;
class ACMChimera;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCMHazardTargetSignature, AActor*, TargetActor);

UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
// 접촉한 팔/다리 또는 몸통 마디에 서버 권한으로 장애물 효과 적용
class CHIMERA_API UCMHazardComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMHazardComponent();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Hazard")
    void NotifyTargetEntered(
        AActor* TargetActor,
        UPrimitiveComponent* TargetComponent = nullptr);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Hazard")
    void NotifyTargetExited(
        AActor* TargetActor,
        UPrimitiveComponent* TargetComponent = nullptr);

    UFUNCTION(BlueprintCallable, Category = "Chimera|Hazard")
    void SetHazardEnabled(bool bEnabled);

    // Definition PDA와 함께 준비된 값 기반 파츠 효과 설정
    void ConfigurePartEffect(const FCMPartObstacleEffectConfig& NewConfig);

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Hazard")
    FCMHazardTargetSignature OnTargetEntered;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Hazard")
    FCMHazardTargetSignature OnTargetExited;

private:
    // 장애물 베이스에서 복사되는 런타임 값이며 Details에서는 직접 편집하지 않음
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Chimera|Hazard",
        meta = (AllowPrivateAccess = "true"))
    FCMPartObstacleEffectConfig PartEffect;

    struct FTrackedPart
    {
        int32 OverlapCount = 0;
    };

    struct FTrackedSegment
    {
        TWeakObjectPtr<ACMChimera> Chimera;
        int32 SegmentIndex = INDEX_NONE;
        int32 OverlapCount = 0;
    };

    ACMPartActorBase* ResolveSupportedPart(
        AActor* TargetActor,
        const UPrimitiveComponent* TargetComponent) const;
    int32 ResolveBodySegment(
        AActor* TargetActor,
        const UPrimitiveComponent* TargetComponent) const;
    void ApplyConfiguredEffect(ACMPartActorBase& PartActor);
    void ApplyConfiguredDamage(ACMChimera& Chimera, int32 SegmentIndex);
    void UpdatePeriodicTimer();
    void HandlePeriodicApplication();

    bool bHazardEnabled = true;
    TMap<TWeakObjectPtr<ACMPartActorBase>, FTrackedPart> TrackedParts;
    TMap<TWeakObjectPtr<UPrimitiveComponent>, FTrackedSegment> TrackedSegments;
    FTimerHandle PeriodicTimerHandle;
};
