#include "Player/CMRuntimeChildActorComponent.h"

#include "Engine/World.h"

void UCMRuntimeChildActorComponent::CreateChildActor(
    TFunction<void(AActor*)> CustomizerFunc)
{
#if WITH_EDITOR
    const UWorld* World = GetWorld();
    if (!World || !World->IsGameWorld())
    {
        DestroyChildActor();
        return;
    }
#endif

    Super::CreateChildActor(MoveTemp(CustomizerFunc));
}
