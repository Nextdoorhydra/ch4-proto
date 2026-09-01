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
    virtual void HandleElementActiveChanged_Implementation(bool bIsActive) override;
    virtual void HandleElementReset_Implementation() override;

    virtual void HandleObstacleActiveStateChanged(bool bIsActive) {}

    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Obstacle")
    void OnObstacleActiveChanged(bool bIsActive);

    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Obstacle")
    void OnObstacleReset();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<UStaticMeshComponent> PrimaryMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<UNiagaraComponent> PrimaryEffect;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<UAudioComponent> LoopAudio;

    // 접촉한 팔과 다리에 코드로 적용할 내구도 및 상태 설정
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Obstacle", meta = (DisplayName = "Part Application"))
    FCMPartObstacleEffectConfig PartEffect;

    // 키메라 전체 ASC에 적용할 GameplayEffect 설정
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Obstacle", meta = (DisplayName = "Chimera Application"))
    FCMChimeraObstacleEffectConfig ChimeraEffect;

    // 피해·상태 표 선택과 인스턴스별 Custom 오버라이드
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Chimera|Obstacle|Balance")
    FCMObstacleBalanceSelection BalanceSelection;

private:
    void ResolveBalance();
    void ApplyResolvedBalance();
    void ApplyComponentActiveState(bool bIsActive);
    void ConfigureDirectEffects();
    void ResetMotionComponents();

    UPROPERTY(Transient)
    FCMResolvedObstacleBalance ResolvedObstacleBalance;
};
