#include "Stage/Trigger/CMVisionStoneBase.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Stage/Trigger/Component/CMActivationTriggerComponent.h"
#include "TimerManager.h"
#include "Vision/CMVisionManagerSubsystem.h"

ACMVisionStoneBase::ACMVisionStoneBase()
{
    // The element stays enabled so the inherited trigger can receive
    // continuous condition changes. Its triggered state is the puzzle state.
    bStartActive = true;
    ActivationTrigger->bOneShot = false;

    VisionPoint = CreateDefaultSubobject<USceneComponent>(TEXT("VisionPoint"));
    VisionPoint->SetupAttachment(SceneRoot);
}

void ACMVisionStoneBase::BeginPlay()
{
    Super::BeginPlay();

    ActivationTrigger->OnActivated.AddDynamic(
        this, &ThisClass::HandleVisionStoneActivated);
    ActivationTrigger->OnDeactivated.AddDynamic(
        this, &ThisClass::HandleVisionStoneDeactivated);

    if (HasAuthority())
    {
        EvaluateVisionCondition();
        GetWorld()->GetTimerManager().SetTimer(
            EvaluationTimerHandle,
            this,
            &ThisClass::EvaluateVisionCondition,
            FMath::Max(EvaluationInterval, 0.02f),
            true);
    }
}

void ACMVisionStoneBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(EvaluationTimerHandle);
    }

    Super::EndPlay(EndPlayReason);
}

void ACMVisionStoneBase::EvaluateVisionCondition()
{
    if (!HasAuthority() || !VisionPoint)
    {
        return;
    }

    UCMVisionManagerSubsystem* VisionManager =
        GetWorld()->GetSubsystem<UCMVisionManagerSubsystem>();
    if (!VisionManager)
    {
        return;
    }

    TArray<UCMVisionComponent*> SeeingSources;
    VisionManager->GetVisionSourcesSeeingLocation(
        VisionPoint->GetComponentLocation(), SeeingSources);
    WatchingPlayerCount = SeeingSources.Num();

    const bool bConditionMet = VisionStoneMode ==
        ECMVisionStoneMode::RequireNoWatchingPlayers
        ? WatchingPlayerCount == 0
        : WatchingPlayerCount >= FMath::Max(RequiredWatchingPlayers, 1);

    if (bConditionMet && !ActivationTrigger->IsTriggered())
    {
        ActivateTrigger(nullptr);
    }
    else if (!bConditionMet && ActivationTrigger->IsTriggered())
    {
        DeactivateTrigger(nullptr);
    }
}

void ACMVisionStoneBase::HandleVisionStoneActivated(AActor* TriggeringActor)
{
    OnVisionStoneStateChanged(true, WatchingPlayerCount);
}

void ACMVisionStoneBase::HandleVisionStoneDeactivated(AActor* TriggeringActor)
{
    OnVisionStoneStateChanged(false, WatchingPlayerCount);
}
