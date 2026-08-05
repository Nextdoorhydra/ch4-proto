#pragma once

#include "CoreMinimal.h"
#include "Chimera/Player/ChimeraControlTypes.h"

struct FChimeraControlAssignmentPolicy
{
    static void Rebalance(
        const TArray<TArray<EChimeraControlPart>>& ExistingAssignments,
        FRandomStream& RandomStream,
        TArray<TArray<EChimeraControlPart>>& OutAssignments
    );
};
