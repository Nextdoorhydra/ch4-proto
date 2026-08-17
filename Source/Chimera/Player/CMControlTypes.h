#pragma once

#include "CoreMinimal.h"

#include "CMControlTypes.generated.h"

/**
 * Stable address of one physical attachment slot on the shared body.
 * ControlBody owns four of these addresses, while the addressed slot may
 * belong to any Segment in the Chimera.
 */
USTRUCT(BlueprintType)
struct CHIMERA_API FCMPartSlotAddress
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 SegmentIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PartSlotIndex = INDEX_NONE;

    bool operator==(const FCMPartSlotAddress& Other) const
    {
        return SegmentIndex == Other.SegmentIndex
            && PartSlotIndex == Other.PartSlotIndex;
    }

    bool operator!=(const FCMPartSlotAddress& Other) const
    {
        return !(*this == Other);
    }
};

FORCEINLINE uint32 GetTypeHash(const FCMPartSlotAddress& Address)
{
    return HashCombine(
        GetTypeHash(Address.SegmentIndex),
        GetTypeHash(Address.PartSlotIndex)
    );
}

namespace CMControl
{
    constexpr int32 PartSlotsPerSegment = 4;
    constexpr int32 MaxKeysPerPlayer = 4;
    constexpr int32 MaxPlayers = 8;
    constexpr int32 MaxSegments = MaxPlayers;
    constexpr int32 MaxPartSlots =
        MaxSegments * PartSlotsPerSegment;

    inline bool IsValidPartSlot(
        const FCMPartSlotAddress& Address,
        int32 SegmentCount = MaxSegments
    )
    {
        return Address.SegmentIndex >= 0
            && Address.SegmentIndex < SegmentCount
            && Address.PartSlotIndex >= 0
            && Address.PartSlotIndex < PartSlotsPerSegment;
    }

    inline int32 ToFlatPartSlotIndex(
        const FCMPartSlotAddress& Address
    )
    {
        return Address.SegmentIndex * PartSlotsPerSegment
            + Address.PartSlotIndex;
    }

    inline FCMPartSlotAddress FromFlatPartSlotIndex(int32 FlatIndex)
    {
        FCMPartSlotAddress Address;
        if (FlatIndex >= 0 && FlatIndex < MaxPartSlots)
        {
            Address.SegmentIndex =
                FlatIndex / PartSlotsPerSegment;
            Address.PartSlotIndex =
                FlatIndex % PartSlotsPerSegment;
        }
        return Address;
    }

    // LineBody currently has prototype leg actions only in local slots 0/1.
    // Slots 2/3 remain valid attachment/control addresses for future Parts.
    inline bool IsPrototypeLegSlot(const FCMPartSlotAddress& Address)
    {
        return Address.PartSlotIndex == 0
            || Address.PartSlotIndex == 1;
    }

    inline bool IsPrototypeRightLeg(const FCMPartSlotAddress& Address)
    {
        return Address.PartSlotIndex == 1;
    }
}
