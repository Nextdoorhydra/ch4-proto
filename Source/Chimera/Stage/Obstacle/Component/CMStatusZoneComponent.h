#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "CMStatusZoneComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCMStatusZoneTargetSignature, AActor*, TargetActor, FGameplayTag, StatusTag);

UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
// 장판 진입과 이탈을 감지해 향후 상태 효과 적용과 해제로 연결
class CHIMERA_API UCMStatusZoneComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMStatusZoneComponent();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Obstacle|StatusZone")
    void NotifyTargetEntered(AActor* TargetActor);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Obstacle|StatusZone")
    void NotifyTargetExited(AActor* TargetActor);

    UFUNCTION(BlueprintCallable, Category = "Chimera|Obstacle|StatusZone")
    void SetZoneEnabled(bool bEnabled) { bZoneEnabled = bEnabled; }

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|StatusZone")
    FGameplayTag StatusTag;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Obstacle|StatusZone")
    FCMStatusZoneTargetSignature OnTargetEntered;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Obstacle|StatusZone")
    FCMStatusZoneTargetSignature OnTargetExited;

private:
    bool bZoneEnabled = true;

    // TODO: GAS 적용 대상과 지속형 GE 핸들을 저장해 이탈 시 정확히 제거
};
