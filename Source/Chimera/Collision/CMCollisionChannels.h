#pragma once

#include "Engine/EngineTypes.h"

namespace CMCollision
{
    // Keep these in sync with Config/DefaultEngine.ini.
    constexpr ECollisionChannel WeaponTrace = ECC_GameTraceChannel1;
    constexpr ECollisionChannel ChimeraHurtbox = ECC_GameTraceChannel2;
}
