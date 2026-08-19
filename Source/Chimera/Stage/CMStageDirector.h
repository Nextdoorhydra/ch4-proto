#pragma once

#include "CoreMinimal.h"
#include "AsyncLoadCompleteMessage.h"
#include "GameFramework/Actor.h"

#include "CMStageDirector.generated.h"

class ACMPlayGameMode;
class ACMPlayGameState;
class UCMStageElementComponent;
class UCMStageSequenceComponent;

struct FCMLocalPendingStageCommand
{
    FName TargetPlacementId;
    FGameplayTag TargetGroup;
    FGameplayTag CommandTag;
    TWeakObjectPtr<AActor> CommandInstigator;
};

UCLASS(Blueprintable)
// 현재 스테이지의 시작 연출·클리어·실패 보고 담당
class CHIMERA_API ACMStageDirector : public AActor
{
    GENERATED_BODY()

public:
    ACMStageDirector();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // 시작 연출을 실행하고 완료 시 FinishStartingPresentation 호출
    UFUNCTION(BlueprintNativeEvent, Category = "Chimera|Stage")
    void BeginStartingPresentation();
    virtual void BeginStartingPresentation_Implementation();

    // 시작 연출 완료를 서버 GameMode에 보고
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Stage")
    void FinishStartingPresentation();

    // 스테이지 클리어 결과 연출을 실행하고 완료 시 FinishResultPresentation 호출
    UFUNCTION(BlueprintNativeEvent, Category = "Chimera|Stage")
    void BeginResultPresentation();
    virtual void BeginResultPresentation_Implementation();

    // 결과 연출 완료를 서버 GameMode에 보고
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Stage")
    void FinishResultPresentation();

    // 현재 스테이지 클리어를 서버 GameMode에 보고
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Stage")
    void CompleteStage();

    // 현재 스테이지 실패를 서버 GameMode에 보고
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Stage")
    void FailStage();

    // 장애물, 퍼즐, 조명 등 선택적 레벨 반응자에게 태그 이벤트 전송
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Stage")
    void BroadcastStageEvent(FGameplayTag EventTag, UObject* EventInstigator);

    // 배치된 요소를 PlacementId와 GroupTag 등록소에 추가
    bool RegisterStageElement(UCMStageElementComponent* Element);
    void UnregisterStageElement(UCMStageElementComponent* Element);

    // PlacementId 또는 GroupTag로 대상을 찾아 명령 전달
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Stage")
    void ExecuteStageCommand(
        FName TargetPlacementId,
        FGameplayTag TargetGroup,
        FGameplayTag CommandTag,
        UObject* CommandInstigator);

    // 각 머신에서 OnDemand 그룹 준비가 끝난 뒤 명령 실행
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Stage")
    void ExecuteStageCommandAfterLoad(
        FName LoadGroupId,
        FName TargetPlacementId,
        FGameplayTag TargetGroup,
        FGameplayTag CommandTag,
        UObject* CommandInstigator);

    // OnDemand 로드 그룹을 모든 머신의 Coordinator에 요청
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chimera|Stage")
    void RequestLoadGroup(FName LoadGroupId);

    UFUNCTION(BlueprintPure, Category = "Chimera|Stage")
    FGuid GetStageInstanceId() const { return StageInstanceId; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UFUNCTION(NetMulticast, Reliable)
    void MulticastExecuteStageCommand(
        FName TargetPlacementId,
        FGameplayTag TargetGroup,
        FGameplayTag CommandTag,
        AActor* CommandInstigator);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastRequestLoadGroup(FName LoadGroupId);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastExecuteStageCommandAfterLoad(
        FName LoadGroupId,
        FName TargetPlacementId,
        FGameplayTag TargetGroup,
        FGameplayTag CommandTag,
        AActor* CommandInstigator);

    ACMPlayGameMode* GetPlayGameMode() const;
    void TryBindPlayGameState();

    UFUNCTION()
    void HandleStagePresentationChanged();

    UFUNCTION()
    void HandleLoadGroupFinished(
        FName LoadGroupId,
        EAsyncLoadResult Result,
        bool bReleasedImmediately);

    void ExecuteStageCommandLocally(
        FName TargetPlacementId,
        FGameplayTag TargetGroup,
        FGameplayTag CommandTag,
        UObject* CommandInstigator);

    UPROPERTY(Transient)
    TObjectPtr<ACMPlayGameState> BoundPlayGameState;

    bool bStartingPresentationFinished = false;
    bool bResultPresentationFinished = false;
    bool bStageResolved = false;
    UPROPERTY(Replicated)
    FGuid StageInstanceId;

    UPROPERTY(VisibleAnywhere, Category = "Chimera|Stage")
    TObjectPtr<UCMStageSequenceComponent> SequenceComponent;

    TMap<FName, TWeakObjectPtr<UCMStageElementComponent>> ElementsByPlacementId;
    TArray<TWeakObjectPtr<UCMStageElementComponent>> RegisteredElements;
    TMap<FName, TArray<FCMLocalPendingStageCommand>> PendingCommandsByLoadGroup;
};
