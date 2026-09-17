#include "CMRoomId.h"

namespace
{
    constexpr TCHAR Letters[] = TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    constexpr TCHAR Digits[] = TEXT("0123456789");
    constexpr TCHAR AlphaNumeric[] =
        TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");

    TCHAR PickRandom(const TCHAR* Characters, int32 CharacterCount)
    {
        return Characters[FMath::RandHelper(CharacterCount)];
    }
}

FString CMRoomId::Generate()
{
    TArray<TCHAR, TInlineAllocator<Length>> Characters;
    Characters.Reserve(Length);
    Characters.Add(PickRandom(Letters, UE_ARRAY_COUNT(Letters) - 1));
    Characters.Add(PickRandom(Digits, UE_ARRAY_COUNT(Digits) - 1));

    while (Characters.Num() < Length)
    {
        Characters.Add(PickRandom(
            AlphaNumeric,
            UE_ARRAY_COUNT(AlphaNumeric) - 1
        ));
    }

    for (int32 Index = Characters.Num() - 1; Index > 0; --Index)
    {
        Characters.Swap(Index, FMath::RandRange(0, Index));
    }

    FString Result;
    Result.Reserve(Length);
    for (const TCHAR Character : Characters)
    {
        Result.AppendChar(Character);
    }
    return Result;
}

bool CMRoomId::NormalizeAndValidate(
    const FString& Input,
    FString& OutRoomId
)
{
    OutRoomId = Input.TrimStartAndEnd().ToUpper();
    if (OutRoomId.Len() != Length)
    {
        OutRoomId.Reset();
        return false;
    }

    bool bHasLetter = false;
    bool bHasDigit = false;
    for (const TCHAR Character : OutRoomId)
    {
        const bool bIsLetter = Character >= TEXT('A')
            && Character <= TEXT('Z');
        const bool bIsDigit = Character >= TEXT('0')
            && Character <= TEXT('9');
        if (!bIsLetter && !bIsDigit)
        {
            OutRoomId.Reset();
            return false;
        }
        bHasLetter |= bIsLetter;
        bHasDigit |= bIsDigit;
    }

    if (!bHasLetter || !bHasDigit)
    {
        OutRoomId.Reset();
        return false;
    }
    return true;
}
