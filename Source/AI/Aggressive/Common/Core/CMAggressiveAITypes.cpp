#include "Aggressive/Common/Core/CMAggressiveAITypes.h"

namespace
{
    constexpr float DirectionSectorDegrees = 45.0f;
    constexpr float DirectionHalfSectorDegrees = DirectionSectorDegrees * 0.5f;
    constexpr float InverseSquareRootTwo = 0.70710678118f;
}

// 몸통 로컬 평면 벡터를 가장 가까운 8방향으로 변환한다.
ECMAggressiveMoveDirection CMAggressiveDirection::QuantizeLocalDirection8(const FVector& LocalDirection, float MinimumPlanarMagnitude)
{
    const FVector2D PlanarDirection(LocalDirection.X, LocalDirection.Y);
    const float SafeMinimumMagnitude = FMath::Max(MinimumPlanarMagnitude, 0.0f);
    if (PlanarDirection.SizeSquared() <= FMath::Square(SafeMinimumMagnitude))
        return ECMAggressiveMoveDirection::None;

    const float AngleDegrees = FMath::RadiansToDegrees(FMath::Atan2(PlanarDirection.Y, PlanarDirection.X));
    const float ShiftedAngle = FMath::Fmod(AngleDegrees + DirectionHalfSectorDegrees + 360.0f, 360.0f);
    const int32 SectorIndex = FMath::FloorToInt(ShiftedAngle / DirectionSectorDegrees);

    return static_cast<ECMAggressiveMoveDirection>(SectorIndex);
}

// 월드 평면 방향을 몸통 Yaw 기준의 로컬 8방향으로 변환한다.
ECMAggressiveMoveDirection CMAggressiveDirection::QuantizeWorldDirection8(const FVector& WorldDirection, const FQuat& BodyRotation, float MinimumPlanarMagnitude)
{
    const float BodyYawRadians = FMath::DegreesToRadians(BodyRotation.Rotator().Yaw);
    const FQuat BodyYawRotation(FVector::UpVector, BodyYawRadians);
    const FVector LocalDirection = BodyYawRotation.UnrotateVector(WorldDirection);

    return QuantizeLocalDirection8(LocalDirection, MinimumPlanarMagnitude);
}

// 8방향 열거형을 정규화된 로컬 평면 벡터로 변환한다.
FVector CMAggressiveDirection::ToLocalUnitVector(ECMAggressiveMoveDirection Direction)
{
    switch (Direction)
    {
    case ECMAggressiveMoveDirection::Forward:
        return FVector::ForwardVector;
    case ECMAggressiveMoveDirection::ForwardRight:
        return FVector(InverseSquareRootTwo, InverseSquareRootTwo, 0.0f);
    case ECMAggressiveMoveDirection::Right:
        return FVector::RightVector;
    case ECMAggressiveMoveDirection::BackwardRight:
        return FVector(-InverseSquareRootTwo, InverseSquareRootTwo, 0.0f);
    case ECMAggressiveMoveDirection::Backward:
        return FVector::BackwardVector;
    case ECMAggressiveMoveDirection::BackwardLeft:
        return FVector(-InverseSquareRootTwo, -InverseSquareRootTwo, 0.0f);
    case ECMAggressiveMoveDirection::Left:
        return -FVector::RightVector;
    case ECMAggressiveMoveDirection::ForwardLeft:
        return FVector(InverseSquareRootTwo, -InverseSquareRootTwo, 0.0f);
    default:
        return FVector::ZeroVector;
    }
}

// 공격적 AI 이동 방향의 로그용 한글 이름을 반환한다.
const TCHAR* CMAggressiveDirection::GetKoreanDisplayName(ECMAggressiveMoveDirection Direction)
{
    switch (Direction)
    {
    case ECMAggressiveMoveDirection::Forward:
        return TEXT("전방");
    case ECMAggressiveMoveDirection::ForwardRight:
        return TEXT("전방 오른쪽");
    case ECMAggressiveMoveDirection::Right:
        return TEXT("오른쪽");
    case ECMAggressiveMoveDirection::BackwardRight:
        return TEXT("후방 오른쪽");
    case ECMAggressiveMoveDirection::Backward:
        return TEXT("후방");
    case ECMAggressiveMoveDirection::BackwardLeft:
        return TEXT("후방 왼쪽");
    case ECMAggressiveMoveDirection::Left:
        return TEXT("왼쪽");
    case ECMAggressiveMoveDirection::ForwardLeft:
        return TEXT("전방 왼쪽");
    default:
        return TEXT("없음");
    }
}
