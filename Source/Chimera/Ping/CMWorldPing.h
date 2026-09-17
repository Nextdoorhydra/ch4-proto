#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ping/CMPingTypes.h"

#include "CMWorldPing.generated.h"

class UCMWorldPingWidget;
class UCMPingGroundWidget;
class USceneComponent;
class UWidgetComponent;

/** Short-lived, server-spawned world marker replicated to every client. */
UCLASS(NotBlueprintable)
class CHIMERA_API ACMWorldPing : public AActor
{
    GENERATED_BODY()

public:
    ACMWorldPing();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    void InitializePing(
        ECMPingType Type,
        const FString& PlayerName,
        const FLinearColor& PlayerColor);

    static void EnforceServerLimit(UWorld& World);

private:
    UFUNCTION()
    void OnRep_Presentation();

    void ApplyPresentation();

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UWidgetComponent> MarkerWidgetComponent;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UWidgetComponent> GroundWidgetComponent;

    UPROPERTY(ReplicatedUsing = OnRep_Presentation)
    ECMPingType PingType = ECMPingType::GoHere;

    UPROPERTY(ReplicatedUsing = OnRep_Presentation)
    FString PingingPlayerName;

    UPROPERTY(ReplicatedUsing = OnRep_Presentation)
    FLinearColor PingingPlayerColor = FLinearColor::White;
};
