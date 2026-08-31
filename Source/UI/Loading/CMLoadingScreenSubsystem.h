#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Tickable.h"

#include "CMLoadingScreenSubsystem.generated.h"

class UCMLoadingScreenWidget;

UCLASS()
class UI_API UCMLoadingScreenSubsystem
    : public ULocalPlayerSubsystem
    , public FTickableGameObject
{
    GENERATED_BODY()

public:
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override;
    virtual UWorld* GetTickableGameObjectWorld() const override;

private:
    void ShowLoadingScreen(UWorld* World);
    void RemoveLoadingScreen();

    UPROPERTY(Transient)
    TObjectPtr<UCMLoadingScreenWidget> LoadingScreenWidget;

    TWeakObjectPtr<UWorld> LoadingScreenWorld;
};
