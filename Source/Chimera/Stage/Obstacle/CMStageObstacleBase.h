#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"

#include "CMStageObstacleBase.generated.h"

class USceneComponent;
class UAudioComponent;
class UCMObstacleDefinition;
class UCMObstacleDefinitionComponent;
class UCMStageElementComponent;
class UNiagaraComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
// 스테이지 장애물의 공통 활성화, 비활성화, 초기화 흐름 담당
class CHIMERA_API ACMStageObstacleBase : public AActor
{
    GENERATED_BODY()

public:
    ACMStageObstacleBase();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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
    bool IsObstacleActive() const { return bObstacleActive; }

    UFUNCTION(BlueprintPure, Category = "Chimera|Obstacle")
    bool IsObstacleDefinitionReady() const { return bDefinitionReady; }

protected:
    virtual void BeginPlay() override;

    // 활성 상태가 바뀔 때 C++ 또는 블루프린트 구현에 전달
    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Obstacle")
    void OnObstacleActiveChanged(bool bIsActive);

    // 초기화 요청을 C++ 또는 블루프린트 구현에 전달
    UFUNCTION(BlueprintImplementableEvent, Category = "Chimera|Obstacle")
    void OnObstacleReset();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Stage")
    TObjectPtr<UCMStageElementComponent> StageElement;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Definition")
    TObjectPtr<UCMObstacleDefinitionComponent> DefinitionComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Presentation")
    TObjectPtr<UStaticMeshComponent> PrimaryMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Presentation")
    TObjectPtr<UNiagaraComponent> PrimaryEffect;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle|Presentation")
    TObjectPtr<UAudioComponent> LoopAudio;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chimera|Obstacle")
    bool bStartActive = true;

private:
    // StageDirector 명령을 장애물 공통 동작으로 변환
    UFUNCTION()
    void HandleStageCommand(FGameplayTag CommandTag, UObject* CommandInstigator);

    // 준비된 Definition의 외형과 게임플레이 설정을 장애물에 적용
    UFUNCTION()
    void HandleDefinitionReady(UCMObstacleDefinition* LoadedDefinition);

    // Definition 실패 시 보이지 않는 판정이 남지 않도록 비활성 유지
    UFUNCTION()
    void HandleDefinitionFailed();

    // 복제된 활성 상태를 클라이언트 표현에 반영
    UFUNCTION()
    void OnRep_ObstacleActive();

    // 요청 상태와 Definition 준비 상태를 실제 활성 상태로 계산
    void UpdateEffectiveActiveState();

    // 서버에서 실제 활성 상태를 변경하고 모든 머신의 표현을 갱신
    void SetObstacleActive(bool bNewActive);

    // 장애물 활성 상태를 부착된 기능 컴포넌트와 기본 연출에 전달
    void ApplyComponentActiveState(bool bIsActive);

    // Definition의 Soft Asset이 LoadGroup에서 모두 준비됐는지 검증 후 적용
    bool ApplyDefinitionAssets(UCMObstacleDefinition* LoadedDefinition);

    // 부착된 공통 이동 컴포넌트를 최초 배치 상태로 복원
    void ResetMotionComponents();

    UPROPERTY(ReplicatedUsing = OnRep_ObstacleActive)
    bool bObstacleActive = false;

    bool bActivationRequested = false;
    bool bDefinitionReady = false;
};
