#include "Stage/CMStageElementComponent.h"

#include "Stage/CMStageDirector.h"
#include "AsyncLoad/CMStageLoadLog.h"
#include "EngineUtils.h"

UCMStageElementComponent::UCMStageElementComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UCMStageElementComponent::BeginPlay()
{
    Super::BeginPlay();

    ACMStageDirector* FoundDirector = nullptr;
    int32 DirectorCount = 0;
    for (TActorIterator<ACMStageDirector> It(GetWorld()); It; ++It)
    {
        FoundDirector = *It;
        ++DirectorCount;
    }

    if (DirectorCount != 1 || !FoundDirector)
    {
        UE_LOG(LogChimeraStageLoad, Error,
            TEXT("StageElement requires exactly one StageDirector. Actor=%s DirectorCount=%d"),
            *GetNameSafe(GetOwner()), DirectorCount);
        return;
    }

    if (FoundDirector->RegisterStageElement(this))
    {
        RegisteredDirector = FoundDirector;
    }
}

void UCMStageElementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (RegisteredDirector.IsValid())
    {
        RegisteredDirector->UnregisterStageElement(this);
    }
    Super::EndPlay(EndPlayReason);
}

bool UCMStageElementComponent::CanExecuteOnCurrentMachine() const
{
    return ExecutionPolicy == ECMStageCommandExecutionPolicy::AllMachines
        || (GetOwner() && GetOwner()->HasAuthority());
}

void UCMStageElementComponent::BroadcastStageEvent(FGameplayTag EventTag)
{
    if (RegisteredDirector.IsValid() && GetOwner() && GetOwner()->HasAuthority())
    {
        RegisteredDirector->BroadcastStageEvent(EventTag, GetOwner());
    }
}

void UCMStageElementComponent::ExecuteStageCommand_Implementation(
    FGameplayTag CommandTag,
    UObject* CommandInstigator)
{
}
