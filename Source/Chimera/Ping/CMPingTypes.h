#pragma once

#include "CoreMinimal.h"

#include "CMPingTypes.generated.h"

UENUM(BlueprintType)
enum class ECMPingType : uint8
{
    GoHere,
    LookHere,
    SwapParts
};

namespace CMPing
{
    inline constexpr float DisplayDuration = 3.0f;
    inline constexpr int32 MaxActivePings = 6;
    inline constexpr float SelectionDeadZone = 28.0f;
    inline constexpr float MaxTraceDistance = 10000.0f;
    inline constexpr float MaxTraceOriginDistanceFromChimera = 5000.0f;

    CHIMERA_API bool TrySelectTypeFromDrag(
        const FVector2D& Drag,
        ECMPingType& OutType);

    CHIMERA_API FLinearColor GetTypeColor(ECMPingType Type);

    CHIMERA_API int32 FindOldestAgeIndex(
        TConstArrayView<float> Ages);
}
