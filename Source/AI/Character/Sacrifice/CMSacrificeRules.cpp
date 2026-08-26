#include "Character/Sacrifice/CMSacrificeRules.h"

TArray<ECMBodyPart> FCMSacrificeRules::GetSeverableBodyParts()
{
    return {
        ECMBodyPart::Head,
        ECMBodyPart::ArmLeft,
        ECMBodyPart::ArmRight,
        ECMBodyPart::LegLeft,
        ECMBodyPart::LegRight
    };
}

uint8 FCMSacrificeRules::GetBodyPartBit(const ECMBodyPart BodyPart)
{
    const uint8 Index = static_cast<uint8>(BodyPart);
    return BodyPart == ECMBodyPart::None || Index >= 8
        ? 0
        : static_cast<uint8>(1u << Index);
}

float FCMSacrificeRules::GetInitialBleedDuration(
    const int32 MissingPartCount
)
{
    return FMath::Max(
        60.0f - static_cast<float>(FMath::Max(MissingPartCount - 1, 0))
            * 15.0f,
        0.0f
    );
}

float FCMSacrificeRules::GetAdditionalBleedReduction(
    const int32 NewlyMissingPartCount
)
{
    return static_cast<float>(FMath::Max(NewlyMissingPartCount, 0)) * 15.0f;
}

ECMBodyPart FCMSacrificeRules::SelectRandomPart(
    const TArray<ECMBodyPart>& AvailableParts,
    FRandomStream& RandomStream
)
{
    return AvailableParts.IsEmpty()
        ? ECMBodyPart::None
        : AvailableParts[RandomStream.RandRange(0, AvailableParts.Num() - 1)];
}
