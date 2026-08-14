#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"

#include "CMGameState.generated.h"

class ACMChimera;
class APlayerState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FChimeraSharedPawnChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FChimeraLobbyRosterChanged);

UCLASS()
class CHIMERA_API ACMGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    virtual void AddPlayerState(APlayerState* PlayerState) override;
    virtual void RemovePlayerState(APlayerState* PlayerState) override;

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    void SetSharedChimera(ACMChimera* NewSharedChimera);
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
    TObjectPtr<ACMChimera> SharedChimera;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Shared Pawn")
    FChimeraSharedPawnChanged OnSharedChimeraChanged;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Lobby")
    FChimeraLobbyRosterChanged OnLobbyRosterChanged;

private:
    UFUNCTION()
    void OnRep_SharedChimera();

};
