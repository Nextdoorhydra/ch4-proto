#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "PrototypeGameMode.generated.h"

class AChimeraPlayerState;
class AChimeraPrototypePawn;

UCLASS()
class CHIMERA_API APrototypeGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    APrototypeGameMode();

    virtual void BeginPlay() override;
    virtual void Logout(AController* Exiting) override;
    virtual void RestartPlayer(AController* NewPlayer) override;
    virtual void GenericPlayerInitialization(AController* C) override;

    bool TryRetryGame(APlayerController* RequestingPlayer);

private:
    bool IsGameplayMap() const;
    void AssignPlayerColors();
    AChimeraPrototypePawn* EnsureSharedChimera();
    void RebalanceControlAssignments(
        const AChimeraPlayerState* ExcludedPlayerState = nullptr
    );

    bool bRetryInProgress = false;
};
