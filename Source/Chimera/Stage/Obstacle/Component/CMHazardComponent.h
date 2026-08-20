#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "CMHazardComponent.generated.h"

UENUM(BlueprintType)
enum class ECMHazardApplicationMode : uint8
{
    Single,   // 진입 시 한 번 적용
    Periodic, // 머무르는 동안 주기적으로 적용 예정
    Kill      // 즉시 처치 처리 예정
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCMHazardTargetSignature, AActor*, TargetActor);

class UGameplayEffect;

UCLASS(ClassGroup = (Chimera), meta = (BlueprintSpawnableComponent))
// 장애물 접촉 대상을 감지하고 향후 데미지 또는 처치 처리로 연결
class CHIMERA_API UCMHazardComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCMHazardComponent();

    // 충돌 컴포넌트에서 감지한 진입 대상을 전달
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Obstacle|Hazard")
    void NotifyTargetEntered(AActor* TargetActor);

    // 충돌 컴포넌트에서 감지한 이탈 대상을 전달
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Obstacle|Hazard")
    void NotifyTargetExited(AActor* TargetActor);

    UFUNCTION(BlueprintCallable, Category = "Chimera|Obstacle|Hazard")
    void SetHazardEnabled(bool bEnabled) { bHazardEnabled = bEnabled; }

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Obstacle|Hazard")
    FCMHazardTargetSignature OnTargetEntered;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Obstacle|Hazard")
    FCMHazardTargetSignature OnTargetExited;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Hazard")
    ECMHazardApplicationMode ApplicationMode = ECMHazardApplicationMode::Single;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Hazard", meta = (ClampMin = "0.01"))
    float PeriodSeconds = 1.0f;

    // Definition PDA에서 준비된 효과 설정을 동기 로드 없이 적용
    void ConfigureHazard(
        TSubclassOf<UGameplayEffect> NewGameplayEffectClass,
        FGameplayTag NewEffectTag,
        ECMHazardApplicationMode NewApplicationMode,
        float NewPeriodSeconds);

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Chimera|Obstacle|Hazard")
    TSubclassOf<UGameplayEffect> GameplayEffectClass;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Chimera|Obstacle|Hazard")
    FGameplayTag EffectTag;

private:
    bool bHazardEnabled = true;

    // TODO: 몸통 마디 판별 뒤 GAS의 데미지 GE 또는 즉시 처치 규칙 적용
};
