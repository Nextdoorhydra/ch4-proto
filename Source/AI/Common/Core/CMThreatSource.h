#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "CMThreatSource.generated.h"

UINTERFACE(MinimalAPI)
class UCMThreatSource : public UInterface
{
    GENERATED_BODY()
};

/** Runtime marker queried by Sacrifice perception. */
class AI_API ICMThreatSource
{
    GENERATED_BODY()

public:
    virtual bool IsThreatActive() const
    {
        return true;
    }
};
