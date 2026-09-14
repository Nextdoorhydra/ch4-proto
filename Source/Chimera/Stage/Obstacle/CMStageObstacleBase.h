#pragma once

#include "CoreMinimal.h"
#include "Stage/CMStageElementBase.h"
#include "Stage/Obstacle/Data/CMObstacleEffectTypes.h"

#include "CMStageObstacleBase.generated.h"

class UAudioComponent;
class UNiagaraComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
// 스테이지 장애물의 공통 활성화, 비활성화, 초기화 흐름 담당
class CHIMERA_API ACMStageObstacleBase : public ACMStageElementBase
{
    GENERATED_BODY()

public:
    ACMStageObstacleBase();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Obstacle")
    void ActivateObstacle();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Obstacle")
    void DeactivateObstacle();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Obstacle")
    void ResetObstacle();

    UFUNCTION(BlueprintPure, Category = "Chimera|Obstacle")
    bool IsObstacleActive() const { return IsElementActive(); }

    UFUNCTION(BlueprintPure, Category = "Chimera|Obstacle|Balance")
    FCMResolvedObstacleBalance GetResolvedObstacleBalance() const
    {
        return ResolvedObstacleBalance;
    }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void HandleElementActivationRequested() override;
    virtual void HandleElementActiveChanged_Implementation(bool bIsActive) override;
    virtual void HandleElementReset_Implementation() override;

    virtual void HandleObstacleActiveStateChanged(bool bIsActive) {}
    virtual bool ShouldManagePrimaryEffectAutomatically() const { return true; }

    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Obstacle")
    void OnObstacleActiveChanged(bool bIsActive);

    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Obstacle")
    void OnObstacleReset();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<UStaticMeshComponent> PrimaryMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<UNiagaraComponent> PrimaryEffect;

    // 지정된 장애물만 CMSound 카탈로그의 활성 루프를 사용한다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chimera|Obstacle|Sound")
    FGameplayTag ActiveLoopSoundTag;

    // 접촉한 팔과 다리에 코드로 적용할 내구도 및 상태 설정
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Obstacle", meta = (DisplayName = "Part Application"))
    FCMPartObstacleEffectConfig PartEffect;

    // 머리 DamageHurtbox에 적용할 시야각·시야거리 감소 설정
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Obstacle", meta = (DisplayName = "Head Vision Application"))
    FCMHeadVisionObstacleEffectConfig HeadVisionEffect;

    // 몸통 마디에 닿은 플레이어의 Q/W/E/R 매핑에 적용할 혼란·착란
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Obstacle", meta = (DisplayName = "Control Status Application"))
    FCMControlObstacleEffectConfig ControlEffect;

    // 피해 표 선택과 인스턴스별 Custom 오버라이드
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Obstacle|Balance")
    FCMObstacleBalanceSelection BalanceSelection;

private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FCMObstacleKillOnEnterTest;
#endif
    void ResolveBalance();
    void ApplyResolvedBalance();
    void ApplyComponentActiveState(bool bIsActive);
    void HandleSoundCatalogsRebuilt();
    void TryStartActiveLoopSound();
    void StopActiveLoopSound();
    void ConfigureDirectEffects();
    void ResetMotionComponents();

    UPROPERTY(Transient)
    FCMResolvedObstacleBalance ResolvedObstacleBalance;

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> ActiveLoopSoundComponent;
};
