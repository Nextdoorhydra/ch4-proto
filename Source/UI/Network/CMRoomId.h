#pragma once

#include "CoreMinimal.h"

namespace CMRoomId
{
    constexpr int32 Length = 6;

    inline FName AttributeKey()
    {
        static const FName Key(TEXT("ROOM_ID"));
        return Key;
    }

    UI_API FString Generate();
    UI_API bool NormalizeAndValidate(
        const FString& Input,
        FString& OutRoomId
    );
}
