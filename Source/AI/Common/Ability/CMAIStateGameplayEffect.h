#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"

#include "CMAIStateGameplayEffect.generated.h"

/** Empty infinite GE; callers add the concrete replicated state tag to its spec. */
UCLASS()
class AI_API UCMAIStateGameplayEffect : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UCMAIStateGameplayEffect();
};
