#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "CMCheckpointResettable.generated.h"

UINTERFACE(MinimalAPI)
class UCMCheckpointResettable : public UInterface
{
    GENERATED_BODY()
};

// 체크포인트 다시하기에서 레벨 배치 상태를 직접 복원해야 하는 액터
class CHIMERA_API ICMCheckpointResettable
{
    GENERATED_BODY()

public:
    virtual void ResetForCheckpoint() = 0;
};
