#include "Sacrifice/CMSacrificeRules.h"

TArray<ECMBodyPart> FCMSacrificeRules::GetSeverableBodyParts()
{
    return {ECMBodyPart::Head, ECMBodyPart::ArmLeft, ECMBodyPart::ArmRight, ECMBodyPart::LegLeft, ECMBodyPart::LegRight};
}

uint8 FCMSacrificeRules::GetBodyPartBit(const ECMBodyPart BodyPart)
{
    const uint8 Index = static_cast<uint8>(BodyPart);

    return BodyPart == ECMBodyPart::None || Index >= 8 ? 0 : static_cast<uint8>(1u << Index);
}

float FCMSacrificeRules::GetInitialBleedDuration(const int32 MissingPartCount)
{
    return FMath::Max(60.0f - static_cast<float>(FMath::Max(MissingPartCount - 1, 0)) * 15.0f, 0.0f);
}

float FCMSacrificeRules::GetAdditionalBleedReduction(const int32 NewlyMissingPartCount)
{
    return static_cast<float>(FMath::Max(NewlyMissingPartCount, 0)) * 15.0f;
}

// 캐릭터 전방과 충격 방향의 평면 내적을 이용해 앞·뒤 넘어짐을 선택한다.
ECMSacrificeHitReactionDirection FCMSacrificeRules::SelectHitReactionDirection(const FVector& ActorForward, const FVector& ImpactDirection)
{
    const FVector Forward2D = ActorForward.GetSafeNormal2D(SMALL_NUMBER, FVector::ForwardVector);
    const FVector Impact2D = ImpactDirection.GetSafeNormal2D(SMALL_NUMBER, -Forward2D);

    return FVector::DotProduct(Forward2D, Impact2D) < 0.0f ? ECMSacrificeHitReactionDirection::Front : ECMSacrificeHitReactionDirection::Back;
}

bool FCMSacrificeRules::ShouldUseInjuredCrawlAfterHit(const bool bWasBackCrawling, const bool bHasLostArmOrLeg)
{
    return bWasBackCrawling || bHasLostArmOrLeg;
}

// 비대칭 상·하 각도와 수평 반각 및 거리를 적용해 시야 원뿔 포함 여부를 판정한다.
bool FCMSacrificeRules::IsPointInsideVisionCone(const FVector& Origin, const FVector& Forward, const float DistanceCm, const float HalfYawDegrees, const float UpDegrees, const float DownDegrees, const FVector& Point)
{
    const FVector ToPoint = Point - Origin;
    if (ToPoint.SizeSquared() > FMath::Square(FMath::Max(DistanceCm, 0.0f)))
    {
        return false;
    }
    if (ToPoint.IsNearlyZero())
    {
        return true;
    }

    const FRotator ViewRotation = Forward.GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector).Rotation();
    const FVector LocalDirection = ViewRotation.UnrotateVector(ToPoint.GetSafeNormal());
    const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(LocalDirection.Y, LocalDirection.X));
    const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(LocalDirection.Z, FVector2D(LocalDirection.X, LocalDirection.Y).Size()));

    return FMath::Abs(Yaw) <= FMath::Max(HalfYawDegrees, 0.0f) && Pitch <= FMath::Max(UpDegrees, 0.0f) && Pitch >= -FMath::Max(DownDegrees, 0.0f);
}

// 16개 후보 중 모든 위협과의 최소 거리를 최대화하고 동률이면 기존 진행 방향을 유지한다.
FVector FCMSacrificeRules::CalculateFleeDirection(const FVector& SacrificeLocation, const TArray<FVector>& ThreatLocations, const FVector& PreferredDirection, const float ProbeDistanceCm)
{
    if (ThreatLocations.IsEmpty())
    {
        return PreferredDirection.GetSafeNormal2D(SMALL_NUMBER, FVector::ForwardVector);
    }

    constexpr int32 DirectionCount = 16;

    const FVector Preferred = PreferredDirection.GetSafeNormal2D();

    FVector BestDirection = FVector::ForwardVector;
    float BestClearanceSq = -1.0f;
    float BestContinuity = -2.0f;

    for (int32 Index = 0; Index < DirectionCount; ++Index)
    {
        const float AngleDegrees = 360.0f * static_cast<float>(Index) / static_cast<float>(DirectionCount);

        const FVector Direction = FVector::ForwardVector.RotateAngleAxis(AngleDegrees, FVector::UpVector);

        const FVector CandidateLocation = SacrificeLocation + Direction * ProbeDistanceCm;

        float MinThreatDistanceSq = TNumericLimits<float>::Max();

        for (const FVector& ThreatLocation : ThreatLocations)
        {
            MinThreatDistanceSq = FMath::Min(MinThreatDistanceSq, FVector::DistSquared2D(CandidateLocation, ThreatLocation));
        }

        const float Continuity = Preferred.IsNearlyZero() ? 0.0f : FVector::DotProduct(Direction, Preferred);

        const bool bBetterClearance = MinThreatDistanceSq > BestClearanceSq + 1.0f;

        const bool bSameClearance = FMath::IsNearlyEqual(MinThreatDistanceSq, BestClearanceSq, 1.0f);

        if (bBetterClearance || (bSameClearance && Continuity > BestContinuity))
        {
            BestClearanceSq = MinThreatDistanceSq;
            BestContinuity = Continuity;
            BestDirection = Direction;
        }
    }

    return BestDirection;
}

// 후보 이동이 유효한 모든 위협과의 거리를 실제로 증가시키는지 확인한다.
bool FCMSacrificeRules::IsDirectionAwayFromAllThreats(const FVector& SacrificeLocation, const TArray<FVector>& ThreatLocations, const FVector& Direction, const float DistanceCm)
{
    const FVector Direction2D = Direction.GetSafeNormal2D();

    if (Direction2D.IsNearlyZero() || ThreatLocations.IsEmpty())
    {
        return false;
    }

    const FVector CandidateLocation = SacrificeLocation + Direction2D * DistanceCm;

    bool bHasThreat = false;

    for (const FVector& ThreatLocation : ThreatLocations)
    {
        const float CurrentDistance = FVector::Dist2D(SacrificeLocation, ThreatLocation);
        const float CandidateDistance = FVector::Dist2D(CandidateLocation, ThreatLocation);

        if (CurrentDistance <= KINDA_SMALL_NUMBER)
        {
            continue;
        }

        bHasThreat = true;

        // 이 방향으로 이동했는데 해당 적에게 더 가까워진다면 실패.
        if (CandidateDistance <= CurrentDistance)
        {
            return false;
        }
    }

    return bHasThreat;
}

ECMBodyPart FCMSacrificeRules::SelectRandomPart(const TArray<ECMBodyPart>& AvailableParts, FRandomStream& RandomStream)
{
    return AvailableParts.IsEmpty() ? ECMBodyPart::None : AvailableParts[RandomStream.RandRange(0, AvailableParts.Num() - 1)];
}
