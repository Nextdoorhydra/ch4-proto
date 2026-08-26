#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Tickable.h"

#include "CMControlHUDSubsystem.generated.h"

class UCMControlHUDWidget;
enum class ENKMUIAsyncResult : uint8;

/** Pushes the local player's control HUD onto the NKMUI HUD layer. */
UCLASS()
class UI_API UCMControlHUDSubsystem
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
    void HandlePolicyInitialized(ENKMUIAsyncResult Result);

    UPROPERTY(Transient)
    TObjectPtr<UCMControlHUDWidget> ControlHUDWidget;

    bool bPolicyInitializationPending = false;
};
