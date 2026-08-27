#pragma once

#include "CoreMinimal.h"
#include "Gore/CMDismembermentDefinition.h"

struct AI_API FCMSacrificeRules
{
    static TArray<ECMBodyPart> GetSeverableBodyParts();
    static uint8 GetBodyPartBit(ECMBodyPart BodyPart);
    static float GetInitialBleedDuration(int32 MissingPartCount);
    static float GetAdditionalBleedReduction(int32 NewlyMissingPartCount);
    static ECMBodyPart SelectRandomPart(
        const TArray<ECMBodyPart>& AvailableParts,
        FRandomStream& RandomStream
    );
};
