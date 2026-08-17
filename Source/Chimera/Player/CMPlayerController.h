#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Player/CMControlTypes.h"

#include "CMPlayerController.generated.h"

class ACMPawn;
class UInputMappingContext;
class UCMClientStageLoadComponent;

UCLASS()
class CHIMERA_API ACMPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ACMPlayerController();

    UFUNCTION(BlueprintPure, Category = "Chimera|Game")
    bool CanRequestRetryGame() const;

    UFUNCTION(BlueprintCallable, Category = "Chimera|Game")
    void RequestRetryGame();

    UFUNCTION(BlueprintCallable, Category = "Chimera|Lobby")
    void RequestStartCampaign();

    // 로컬 로드 컴포넌트의 결과를 서버 RPC로 전달
    void ReportLocalStageLoadComplete(FGuid RequestId, bool bSucceeded);

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float DeltaTime) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    int32 MappingPriority = 0;

private:
    void FirstControlKeyPressed();
    void SecondControlKeyPressed();
    void ThirdControlKeyPressed();
    void FourthControlKeyPressed();
    void FirstControlKeyReleased();
    void SecondControlKeyReleased();
    void ThirdControlKeyReleased();
    void FourthControlKeyReleased();
    void SetControlSlotPressed(int32 SlotIndex, bool bPressed);

    void LookYaw(float AxisValue);
    void LookPitch(float AxisValue);
    void ZoomCamera(float AxisValue);

    ACMPawn* GetSharedChimera() const;

    UFUNCTION(Server, Reliable)
    void ServerSetControlSlotPressed(int32 SlotIndex, bool bPressed);

    UFUNCTION(Server, Reliable)
    void ServerRequestRetryGame();

    UFUNCTION(Server, Reliable)
    void ServerRequestStartCampaign();

    UFUNCTION(Server, Reliable)
    void ServerReportStageLoadComplete(FGuid RequestId, bool bSucceeded);

    UPROPERTY(Transient)
    TObjectPtr<ACMPawn> CachedSharedChimera;

    UPROPERTY(Transient)
    TObjectPtr<UCMClientStageLoadComponent> ClientStageLoadComponent;

    FRotator LocalCameraRotation = FRotator::ZeroRotator;
    bool bLocalCameraInitialized = false;

    ECMControlPart PressedControlParts[
        CMControl::MaxKeysPerPlayer
    ];
};
