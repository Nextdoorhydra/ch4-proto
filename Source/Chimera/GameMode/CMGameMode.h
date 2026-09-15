#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "CMGameMode.generated.h"

class ACMPlayerState;
class ACMChimera;
class UCMStageLoadCoordinatorSubsystem;

UCLASS()
class CHIMERA_API ACMGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ACMGameMode();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Logout(AController* Exiting) override;
    virtual void RestartPlayer(AController* NewPlayer) override;
    virtual void GenericPlayerInitialization(AController* C) override;
    virtual UClass* GetDefaultPawnClassForController_Implementation(
        AController* InController
    ) override;

    virtual bool TryRetryGame(APlayerController* RequestingPlayer);

protected:
    // 하위 GameMode가 이탈 플레이어의 배정을 유예할지 결정
    virtual bool ShouldPreservePlayerOnLogout(
        AController* Exiting,
        const ACMPlayerState* ExitingPlayerState) const;

    // 하위 GameMode가 기존 배정을 복원하면 기본 재배정 생략
    virtual bool RestorePreservedControlAssignment(
        AController* NewPlayer,
        ACMChimera* SharedChimera);

    bool IsGameplayMap() const;
    bool IsMainMenuMap() const;
    bool IsSoloTestMode() const;
    void AssignPlayerSlots();
    void AssignPlayerColors();
    ACMChimera* EnsureSharedChimera();
    void RebalanceControlAssignments(
        const ACMPlayerState* ExcludedPlayerState = nullptr
    );

    // 공용 키메라 최초 생성에 사용할 PlayerStartTag이며 비어 있으면 기존 첫 시작점 사용
    UPROPERTY(EditDefaultsOnly, Category = "Chimera|Spawn")
    FName SharedChimeraPlayerStartTag;

    UPROPERTY(EditDefaultsOnly, Category = "Chimera|Main Menu")
    FPrimaryAssetId MainMenuLoadScheduleId;

    // 메인 메뉴에서 화면 구성을 담당하는 동적 스트리밍 레벨
    UPROPERTY(EditDefaultsOnly, Category = "Chimera|Main Menu")
    FName MainMenuPresentationLevelName = TEXT("L_MainMenuSub");

    UPROPERTY(EditDefaultsOnly, Category = "Chimera|Main Menu", meta = (ClampMin = "1.0"))
    float MainMenuPresentationTimeoutSeconds = 30.0f;

private:
    void StartMainMenuAudio();
    void StopMainMenuAudio();
    void CheckMainMenuPresentationReady();
    void TryFinishMainMenuLoading();
    void FinishMainMenuLoading();
    void HandleMainMenuLoadingTimeout();

    UFUNCTION()
    void HandleMainMenuLoadFinished(FGuid RequestId, bool bSucceeded);

    UPROPERTY(Transient)
    TObjectPtr<UCMStageLoadCoordinatorSubsystem> MainMenuLoadCoordinator;

    FGuid MainMenuLoadRequestId;
    FTimerHandle MainMenuPresentationPollTimer;
    FTimerHandle MainMenuPresentationFinishTimer;
    FTimerHandle MainMenuPresentationTimeoutTimer;
    bool bMainMenuAssetsResolved = false;
    bool bMainMenuAssetsLoaded = false;
    bool bMainMenuPresentationReady = false;
    bool bMainMenuFinishScheduled = false;
    bool bRetryInProgress = false;
};
