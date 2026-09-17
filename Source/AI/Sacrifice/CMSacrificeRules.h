#pragma once

#include "CoreMinimal.h"
#include "Gore/CMDismembermentDefinition.h"
#include "Sacrifice/CMSacrificeAITypes.h"

struct AI_API FCMSacrificeRules
{
    static TArray<ECMBodyPart> GetSeverableBodyParts();
    static uint8 GetBodyPartBit(ECMBodyPart BodyPart);
    static float GetInitialBleedDuration(int32 MissingPartCount);
    static float GetAdditionalBleedReduction(int32 NewlyMissingPartCount);
    static ECMSacrificeHitReactionDirection SelectHitReactionDirection(const FVector& ActorForward, const FVector& ImpactDirection);
    static bool ShouldUseInjuredCrawlAfterHit(bool bWasBackCrawling, bool bHasLostArmOrLeg);
    static bool IsMovementActionState(ECMSacrificeActionState ActionState);
    static bool HasMeaningfulProjectedMove(const FVector& StartLocation, const FVector& ProjectedLocation, float AcceptanceRadius, float MinimumTravelDistance);
    static bool IsPointInsideVisionCone(const FVector& Origin, const FVector& Forward, float DistanceCm, float HalfYawDegrees, float UpDegrees, float DownDegrees, const FVector& Point);

    static FVector CalculateFleeDirection(const FVector& SacrificeLocation, const TArray<FVector>& ThreatLocations, const FVector& PreferredDirection, float ProbeDistanceCm);

    static bool IsDirectionAwayFromAllThreats(const FVector& SacrificeLocation, const TArray<FVector>& ThreatLocations, const FVector& Direction, float DistanceCm);
    static ECMBodyPart SelectRandomPart(const TArray<ECMBodyPart>& AvailableParts, FRandomStream& RandomStream);
};
