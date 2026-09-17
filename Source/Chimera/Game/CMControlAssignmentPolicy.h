#pragma once

#include "CoreMinimal.h"
#include "Player/CMControlTypes.h"

struct CHIMERA_API FCMControlAssignmentPolicy
{
    static void Rebalance(
        const TArray<TArray<FCMPartSlotAddress>>& ExistingAssignments,
        int32 ActiveSegmentCount,
        FRandomStream& RandomStream,
        TArray<TArray<FCMPartSlotAddress>>& OutAssignments
    );
};
