#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "CMCheckpointAIResettable.generated.h"

UINTERFACE()
class CHIMERA_API UCMCheckpointAIResettable : public UInterface
{
    GENERATED_BODY()
};

class CHIMERA_API ICMCheckpointAIResettable
{
    GENERATED_BODY()

public:
    virtual bool ResetAIForCheckpoint() = 0;
};
