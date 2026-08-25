#include "Stage/Mechanism/CMVisionStoneBase.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Vision/CMVisionManagerSubsystem.h"
#include "Stage/CMStageElementComponent.h"
#include "Stage/CMStageCommandTags.h"

ACMVisionStoneBase::ACMVisionStoneBase()
{
    bStartActive = false;
    TargetCommandTag = CMStageCommandTags::Mechanism_Activate;

    VisionPoint = CreateDefaultSubobject<USceneComponent>(TEXT("VisionPoint"));
    VisionPoint->SetupAttachment(SceneRoot);
}

void ACMVisionStoneBase::BeginPlay()
{
    Super::BeginPlay();

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

    if (bConditionMet != IsMechanismActive())
    {
        if (bConditionMet)
        {
            ActivateMechanism();
        }
        else
        {
            DeactivateMechanism();
        }
    }
}

void ACMVisionStoneBase::HandleMechanismActiveChanged_Implementation(
    bool bIsActive)
{
    Super::HandleMechanismActiveChanged_Implementation(bIsActive);

    OnVisionStoneStateChanged(bIsActive, WatchingPlayerCount);

    if (HasAuthority() && StageElement && !TargetPlacementId.IsNone())
    {
        StageElement->RequestStageCommand(
            TargetPlacementId,
            TargetGroup,
            bIsActive
                ? TargetCommandTag
                : CMStageCommandTags::Mechanism_Deactivate);
    }
}
