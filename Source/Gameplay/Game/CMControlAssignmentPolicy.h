#pragma once

#include "CoreMinimal.h"
#include "Player/CMControlTypes.h"

struct GAMEPLAY_API FCMControlAssignmentPolicy
{
    static void Rebalance(
        const TArray<TArray<ECMControlPart>>& ExistingAssignments,
        FRandomStream& RandomStream,
        TArray<TArray<ECMControlPart>>& OutAssignments
    );
};
