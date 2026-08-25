#pragma once

#include "CoreMinimal.h"
#include "Stage/CMStageElementBase.h"

#include "CMStageObstacleBase.generated.h"

class UAudioComponent;
class UCMObstacleDefinition;
class UCMObstacleDefinitionComponent;
class UNiagaraComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
// 스테이지 장애물의 공통 활성화, 비활성화, 초기화 흐름 담당
class CHIMERA_API ACMStageObstacleBase : public ACMStageElementBase
{
    GENERATED_BODY()

public:
    ACMStageObstacleBase();
    // 장애물을 작동 가능한 상태로 전환
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Obstacle")
    void ActivateObstacle();

    // 장애물의 작동을 중지
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Obstacle")
    void DeactivateObstacle();

    // 장애물을 레벨 시작 상태로 되돌림
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Obstacle")
    void ResetObstacle();

    UFUNCTION(BlueprintPure, Category = "Chimera|Obstacle")
    bool IsObstacleActive() const { return IsElementActive(); }

    UFUNCTION(BlueprintPure, Category = "Chimera|Obstacle")
    bool IsObstacleDefinitionReady() const { return bDefinitionReady; }

protected:
    virtual void BeginPlay() override;

    // Definition이 준비된 경우에만 활성화 요청을 실제 작동 상태로 전환
    virtual bool CanActivateElement() const override;

    // 공통 활성 상태를 장애물 기능 컴포넌트와 표현에 적용
    virtual void HandleElementActiveChanged_Implementation(bool bIsActive) override;

    // 공통 초기화 요청에서 장애물 이동과 전용 표현 복원
    virtual void HandleElementReset_Implementation() override;

    // 하위 장애물이 공통 활성 상태에 맞춰 전용 Collision과 표현을 갱신
    virtual void HandleObstacleActiveStateChanged(bool bIsActive) {}

    // 활성 상태가 바뀔 때 C++ 또는 블루프린트 구현에 전달
    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Obstacle")
    void OnObstacleActiveChanged(bool bIsActive);

    // 초기화 요청을 C++ 또는 블루프린트 구현에 전달
    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Obstacle")
    void OnObstacleReset();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<UCMObstacleDefinitionComponent> DefinitionComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<UStaticMeshComponent> PrimaryMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<UNiagaraComponent> PrimaryEffect;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Components")
    TObjectPtr<UAudioComponent> LoopAudio;

private:
    // 준비된 Definition의 외형과 게임플레이 설정을 장애물에 적용
    UFUNCTION()
    void HandleDefinitionReady(UCMObstacleDefinition* LoadedDefinition);

    // Definition 실패 시 보이지 않는 판정이 남지 않도록 비활성 유지
    UFUNCTION()
    void HandleDefinitionFailed();

    // 장애물 활성 상태를 부착된 기능 컴포넌트와 기본 연출에 전달
    void ApplyComponentActiveState(bool bIsActive);

    // Definition의 Soft Asset이 LoadGroup에서 모두 준비됐는지 검증 후 적용
    bool ApplyDefinitionAssets(UCMObstacleDefinition* LoadedDefinition);

    // 부착된 공통 이동 컴포넌트를 최초 배치 상태로 복원
    void ResetMotionComponents();

    bool bDefinitionReady = false;
};
