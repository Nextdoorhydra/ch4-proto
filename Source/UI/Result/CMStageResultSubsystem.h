#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Tickable.h"

#include "CMStageResultSubsystem.generated.h"

class UCMStageResultWidget;
enum class ENKMUIAsyncResult : uint8;

UCLASS()
class UI_API UCMStageResultSubsystem
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
    void RemoveResultWidget();

    UPROPERTY(Transient)
    TObjectPtr<UCMStageResultWidget> ResultWidget;

    bool bPolicyInitializationPending = false;
};
