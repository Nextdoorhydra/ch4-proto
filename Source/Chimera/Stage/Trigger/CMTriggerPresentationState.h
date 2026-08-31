#pragma once

#include "CoreMinimal.h"
#include "CMTriggerPresentationState.generated.h"

// Read-only UI snapshot. Gameplay continues to use the authoritative trigger state.
USTRUCT(BlueprintType)
struct CHIMERA_API FCMTriggerPresentationState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) bool bReady = false;
    UPROPERTY(BlueprintReadOnly) bool bEnabled = false;
    UPROPERTY(BlueprintReadOnly) bool bTriggered = false;
    UPROPERTY(BlueprintReadOnly) bool bCanActivate = false;
    UPROPERTY(BlueprintReadOnly) bool bSupportsWeight = false;
    UPROPERTY(BlueprintReadOnly) float CurrentWeight = 0.0f;
    UPROPERTY(BlueprintReadOnly) float RequiredWeight = 0.0f;
    UPROPERTY(BlueprintReadOnly) float ReleaseWeight = 0.0f;

    bool operator==(const FCMTriggerPresentationState& Other) const
    {
        return bReady == Other.bReady && bEnabled == Other.bEnabled
            && bTriggered == Other.bTriggered && bCanActivate == Other.bCanActivate
            && bSupportsWeight == Other.bSupportsWeight
            && CurrentWeight == Other.CurrentWeight && RequiredWeight == Other.RequiredWeight
            && ReleaseWeight == Other.ReleaseWeight;
    }
};
