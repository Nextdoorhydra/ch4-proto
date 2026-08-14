#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "CMGameMode.generated.h"

class ACMPlayerState;
class ACMChimera;

UCLASS()
class CHIMERA_API ACMGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ACMGameMode();

    virtual void BeginPlay() override;
    virtual void Logout(AController* Exiting) override;
    virtual void RestartPlayer(AController* NewPlayer) override;
    virtual void GenericPlayerInitialization(AController* C) override;
    virtual UClass* GetDefaultPawnClassForController_Implementation(
        AController* InController
    ) override;

    bool TryRetryGame(APlayerController* RequestingPlayer);

private:
    bool IsGameplayMap() const;
    void AssignPlayerColors();
    ACMChimera* EnsureSharedChimera();
    void RebalanceControlAssignments(
        const ACMPlayerState* ExcludedPlayerState = nullptr
    );

    bool bRetryInProgress = false;
};
