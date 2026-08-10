#pragma once

#include "CoreMinimal.h"
#include "Player/CMControlTypes.h"

struct FCMControlAssignmentPolicy
{
    static void Rebalance(
        const TArray<TArray<ECMControlPart>>& ExistingAssignments,
        FRandomStream& RandomStream,
        TArray<TArray<ECMControlPart>>& OutAssignments
    );
};
