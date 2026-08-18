#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Player/CMControlTypes.h"

#include "CMPlayerController.generated.h"

class ACMChimera;
class UCMVisionInputComponent;
class UInputAction;
class UInputMappingContext;

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

    /** Console-command entry point. The actual damage is always applied by the server. */
    void RequestCheatKillAllSegments();

    /** Console-command entry point using the zero-based body-segment index. */
    void RequestCheatKillSegment(int32 SegmentIndex);

    void RequestCheatSpawnRandomParts();
    void RequestCheatClearRandomParts();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void SetupInputComponent() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chimera|Vision")
    TObjectPtr<UCMVisionInputComponent> VisionInputComponent;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    int32 MappingPriority = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> FirstControlAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> SecondControlAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> ThirdControlAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> FourthControlAction;

    /** Hold this action while pressing Q/W/E/R to detach that control slot. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> DetachModifierAction;

private:
    void FirstControlKeyPressed();
    void SecondControlKeyPressed();
    void ThirdControlKeyPressed();
    void FourthControlKeyPressed();
    void FirstControlKeyReleased();
    void SecondControlKeyReleased();
    void ThirdControlKeyReleased();
    void FourthControlKeyReleased();
    void DetachModifierPressed();
    void DetachModifierReleased();
    void SetControlSlotPressed(int32 SlotIndex, bool bPressed);

    ACMChimera* GetSharedChimera() const;

    /** SharedChimera가 복제된 순간에만 로컬 ViewTarget을 연결한다. */
    UFUNCTION()
    void HandleSharedChimeraChanged();

    UFUNCTION(Server, Reliable)
    void ServerRequestRetryGame();

    UFUNCTION(Server, Reliable)
    void ServerCheatKillAllSegments();

    UFUNCTION(Server, Reliable)
    void ServerCheatKillSegment(int32 SegmentIndex);

    UFUNCTION(Server, Reliable)
    void ServerCheatSpawnRandomParts();

    UFUNCTION(Server, Reliable)
    void ServerCheatClearRandomParts();

    UPROPERTY(Transient)
    TObjectPtr<ACMChimera> CachedSharedChimera;

    bool bDetachModifierHeld = false;

};
