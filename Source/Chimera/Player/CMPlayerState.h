#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"

#include "CMPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FChimeraPlayerColorChanged);

UCLASS()
class CHIMERA_API ACMPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    ACMPlayerState();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;
    virtual void SetPlayerName(const FString& NewPlayerName) override;
    virtual void OnRep_PlayerName() override;

    void SetPlayerColorIndex(int32 NewPlayerColorIndex);

    UFUNCTION(BlueprintPure, Category = "Chimera|Player")
    FLinearColor GetPlayerColor() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Player")
    int32 GetPlayerColorIndex() const;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Player")
    FChimeraPlayerColorChanged OnPlayerColorChanged;

private:
    UFUNCTION()
    void OnRep_PlayerColorIndex();

    UPROPERTY(ReplicatedUsing = OnRep_PlayerColorIndex)
    int32 PlayerColorIndex = INDEX_NONE;
};
