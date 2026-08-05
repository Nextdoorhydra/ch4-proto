#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ChimeraControlTypes.h"

#include "PrototypePlayerController.generated.h"

class AChimeraPrototypePawn;
class UInputMappingContext;

UCLASS()
class CHIMERA_API APrototypePlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    APrototypePlayerController();

    UFUNCTION(BlueprintPure, Category = "Chimera|Game")
    bool CanRequestRetryGame() const;

    UFUNCTION(BlueprintCallable, Category = "Chimera|Game")
    void RequestRetryGame();

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

    AChimeraPrototypePawn* GetSharedChimera() const;

    UFUNCTION(Server, Reliable)
    void ServerSetControlSlotPressed(int32 SlotIndex, bool bPressed);

    UFUNCTION(Server, Reliable)
    void ServerRequestRetryGame();

    UPROPERTY(Transient)
    TObjectPtr<AChimeraPrototypePawn> CachedSharedChimera;

    FRotator LocalCameraRotation = FRotator::ZeroRotator;
    bool bLocalCameraInitialized = false;

    EChimeraControlPart PressedControlParts[
        ChimeraControl::MaxKeysPerPlayer
    ];
};
