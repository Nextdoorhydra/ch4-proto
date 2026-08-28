#pragma once

#include "CoreMinimal.h"

#include "CMSacrificeAITypes.generated.h"

/** Replicated animation/behavior state. Animation assets can consume this later. */
UENUM(BlueprintType)
enum class ECMSacrificeActionState : uint8
{
    None,
    Prayer,
    LookAround,
    Wander,
    BackFall,
    BackCrawl,
    Flee,
    ExhaustedWalk,
    Vigilant,
    Incapacitated,
    Dead,
    InjuredCrawl,
    GettingUp
};

/** Latched attack side used by the Anim Blueprint during a hit reaction. */
UENUM(BlueprintType)
enum class ECMSacrificeHitReactionDirection : uint8
{
    None,
    Front,
    Back
};
