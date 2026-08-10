#pragma once

#include "CoreMinimal.h"

#include "CMControlTypes.generated.h"

UENUM(BlueprintType)
enum class ECMControlPart : uint8
{
    FirstLeft,
    FirstRight,
    SecondLeft,
    SecondRight,
    ThirdLeft,
    ThirdRight,
    FourthLeft,
    FourthRight,
    None = 255 UMETA(Hidden)
};

namespace CMControl
{
    constexpr int32 MaxControlParts = 8;
    constexpr int32 MaxKeysPerPlayer = 4;
    constexpr int32 MaxPlayers = 8;

    inline bool IsValidPart(ECMControlPart Part)
    {
        return static_cast<uint8>(Part) < MaxControlParts;
    }

    inline int32 GetSegmentIndex(ECMControlPart Part)
    {
        return static_cast<uint8>(Part) / 2;
    }

    inline bool IsRightLeg(ECMControlPart Part)
    {
        return static_cast<uint8>(Part) % 2 == 1;
    }
}
