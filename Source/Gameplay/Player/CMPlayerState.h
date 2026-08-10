#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"

#include "CMControlTypes.h"
#include "CMPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FChimeraControlAssignmentsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FChimeraPlayerColorChanged);

UCLASS()
class GAMEPLAY_API ACMPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    ACMPlayerState();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;
    virtual void SetPlayerName(const FString& NewPlayerName) override;
    virtual void OnRep_PlayerName() override;

    ECMControlPart GetControlPartForSlot(int32 SlotIndex) const;
    void SetAssignedControlParts(
        const TArray<ECMControlPart>& NewAssignments
    );
    void SetPlayerColorIndex(int32 NewPlayerColorIndex);

    UFUNCTION(BlueprintPure, Category = "Chimera|Player")
    FLinearColor GetPlayerColor() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Player")
    int32 GetPlayerColorIndex() const;

    UFUNCTION(BlueprintPure, Category = "Chimera|Controls")
    int32 GetAssignedControlCount() const;

    UPROPERTY(
        ReplicatedUsing = OnRep_AssignedControlParts,
        BlueprintReadOnly,
        Category = "Chimera|Controls"
    )
    TArray<ECMControlPart> AssignedControlParts;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Controls")
    FChimeraControlAssignmentsChanged OnControlAssignmentsChanged;

    UPROPERTY(BlueprintAssignable, Category = "Chimera|Player")
    FChimeraPlayerColorChanged OnPlayerColorChanged;

private:
    UFUNCTION()
    void OnRep_AssignedControlParts();

    UFUNCTION()
    void OnRep_PlayerColorIndex();

    UPROPERTY(ReplicatedUsing = OnRep_PlayerColorIndex)
    int32 PlayerColorIndex = INDEX_NONE;
};
