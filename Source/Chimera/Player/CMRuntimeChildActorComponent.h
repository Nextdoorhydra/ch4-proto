#pragma once

#include "CoreMinimal.h"
#include "Components/ChildActorComponent.h"

#include "CMRuntimeChildActorComponent.generated.h"

/**
 * Child Actor slot that keeps its authored class/template but only instantiates
 * the actor in a game world. This prevents nested Chimera presentation slots
 * from recursively expanding inside Blueprint editor preview worlds.
 */
UCLASS(ClassGroup = (Chimera))
class CHIMERA_API UCMRuntimeChildActorComponent
    : public UChildActorComponent
{
    GENERATED_BODY()

public:
    virtual void CreateChildActor(
        TFunction<void(AActor*)> CustomizerFunc = nullptr) override;
};
