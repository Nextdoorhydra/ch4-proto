#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"

#include "CMGameState.generated.h"

class ACMPawn;
class APlayerState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FChimeraSharedPawnChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FChimeraLobbyRosterChanged);

UCLASS()
class GAMEPLAY_API ACMGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    virtual void AddPlayerState(APlayerState* PlayerState) override;
    virtual void RemovePlayerState(APlayerState* PlayerState) override;

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    void SetSharedChimera(ACMPawn* NewSharedChimera);
    void NotifyLobbyRosterChanged();

    UFUNCTION(BlueprintPure, Category = "Chimera|Lobby")
    int32 GetLobbyPlayerCount() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Lobby")
    int32 GetLobbyMaxPlayers() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Lobby")
    TArray<FString> GetLobbyPlayerNames() const;

    UPROPERTY(
        ReplicatedUsing = OnRep_SharedChimera,
        BlueprintReadOnly,
        Category = "Chimera|Shared Pawn"
    )
    TObjectPtr<ACMPawn> SharedChimera;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Shared Pawn")
    FChimeraSharedPawnChanged OnSharedChimeraChanged;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Lobby")
    FChimeraLobbyRosterChanged OnLobbyRosterChanged;

private:
    UFUNCTION()
    void OnRep_SharedChimera();
};
