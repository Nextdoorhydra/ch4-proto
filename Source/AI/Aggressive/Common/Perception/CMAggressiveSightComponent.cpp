#include "Aggressive/Common/Perception/CMAggressiveSightComponent.h"

#include "DrawDebugHelpers.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameMode/CMGameState.h"
#include "HAL/IConsoleManager.h"
#include "Player/CMChimera.h"

DEFINE_LOG_CATEGORY_STATIC(LogCMAggressiveSight, Log, All);

namespace CMAggressiveSight
{
    constexpr float UpdateInterval = 0.05f;
    constexpr float RedTintStrength = 1.0f;

#if !UE_BUILD_SHIPPING
    bool bDrawSightDebug = false;

    void SetSightDebugDraw(const TArray<FString>& Args, UWorld* World)
    {
        if (Args.Num() != 1)
        {
            return;
        }

        const FString& Value = Args[0];
        if (Value.Equals(TEXT("on"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Value == TEXT("1"))
        {
            bDrawSightDebug = true;
        }
        else if (Value.Equals(TEXT("off"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("false"), ESearchCase::IgnoreCase) || Value == TEXT("0"))
        {
            bDrawSightDebug = false;
        }
        else
        {
            UE_LOG(LogCMAggressiveSight, Warning, TEXT("Invalid value '%s'. Usage: CM.AI.SightDebug on|off"), *Value);

            return;
        }

    }

    FAutoConsoleCommandWithWorldAndArgs SightDebugCommand(TEXT("CM.AI.SightDebug"), TEXT("Draws hostile AI sight cones. Usage: CM.AI.SightDebug on|off"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetSightDebugDraw));
#endif
} // namespace CMAggressiveSight

UCMAggressiveSightComponent::UCMAggressiveSightComponent()
{
    PrimaryComponentTick.TickInterval = CMAggressiveSight::UpdateInterval;
}

// 서버에서도 적대 AI 시야 판정이 계속 실행되도록 시각 설정과 Tick을 초기화한다.
void UCMAggressiveSightComponent::BeginPlay()
{
    Super::BeginPlay();

    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        return;
    }

    // 전용 서버에서는 부모의 시각 보간 Tick이 꺼지므로 권한 시야 판정을 위해 다시 활성화한다.
    SetComponentTickEnabled(true);
    ApplyVisionSettings();
    UpdateAuthoritySight();
}

void UCMAggressiveSightComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    DrawSightDebug();

    if (const AActor* Owner = GetOwner(); Owner && Owner->HasAuthority())
    {
        UpdateAuthoritySight();
    }
}

void UCMAggressiveSightComponent::DrawSightDebug() const
{
#if ENABLE_DRAW_DEBUG && !UE_BUILD_SHIPPING
    UWorld* World = GetWorld();
    if (!CMAggressiveSight::bDrawSightDebug || !World || World->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    const FVector Origin = GetVisionOrigin();
    FVector Forward = GetSightForward();
    Forward.Z = 0.0f;
    Forward = Forward.GetSafeNormal();
    if (Forward.IsNearlyZero())
    {
        Forward = FVector::ForwardVector;
    }
    const float Distance = FMath::Max(SightDistanceCm, 0.0f);
    const float HorizontalAngle = FMath::Clamp(HorizontalSightAngleDegrees, 0.0f, 360.0f);
    const float HorizontalHalfAngle = HorizontalAngle * 0.5f;
    const int32 ArcSegmentCount = FMath::Clamp(FMath::CeilToInt(HorizontalAngle / 5.0f), 1, 72);
    const float Lifetime = CMAggressiveSight::UpdateInterval * 1.5f;

    FVector PreviousArcPoint = Origin + Forward.RotateAngleAxis(-HorizontalHalfAngle, FVector::UpVector) * Distance;
    DrawDebugLine(World, Origin, PreviousArcPoint, FColor::Red, false, Lifetime, 0, 1.5f);
    for (int32 SegmentIndex = 1; SegmentIndex <= ArcSegmentCount; ++SegmentIndex)
    {
        const float AngleDegrees = FMath::Lerp(-HorizontalHalfAngle, HorizontalHalfAngle, static_cast<float>(SegmentIndex) / ArcSegmentCount);
        const FVector ArcPoint = Origin + Forward.RotateAngleAxis(AngleDegrees, FVector::UpVector) * Distance;
        DrawDebugLine(World, PreviousArcPoint, ArcPoint, FColor::Red, false, Lifetime, 0, 1.5f);
        PreviousArcPoint = ArcPoint;
    }
    DrawDebugLine(World, Origin, PreviousArcPoint, FColor::Red, false, Lifetime, 0, 1.5f);
    DrawDebugSphere(World, Origin, 10.0f, 12, FColor::Yellow, false, Lifetime, 0, 1.5f);
    DrawDebugDirectionalArrow(World, Origin, Origin + Forward * Distance, 20.0f, FColor::White, false, Lifetime, 0, 1.5f);
#endif
}

void UCMAggressiveSightComponent::SetSightDefaults(float InSightDistanceCm, float InHorizontalSightAngleDegrees, float InVerticalSightAngleDegrees)
{
    SightDistanceCm = FMath::Max(InSightDistanceCm, 0.0f);
    HorizontalSightAngleDegrees = FMath::Clamp(InHorizontalSightAngleDegrees, 0.0f, 360.0f);
    VerticalSightAngleDegrees = FMath::Clamp(InVerticalSightAngleDegrees, 0.0f, 180.0f);
}

void UCMAggressiveSightComponent::SetSightForwardReversed(bool bInReversed)
{
    bSightForwardReversed = bInReversed;
}

// 거리와 수평·수직 시야각 및 정적 장애물 차폐를 모두 통과한 대상을 감지한다.
bool UCMAggressiveSightComponent::CanSeeActor(const AActor* Target) const
{
    if (!IsValid(Target))
    {
        return false;
    }
    if (CanSeeTargetPoint(*Target, Target->GetActorLocation()))
    {
        return true;
    }

    TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
    Target->GetComponents(PrimitiveComponents);
    const FVector Origin = GetVisionOrigin();
    for (const UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
    {
        if (!PrimitiveComponent || !PrimitiveComponent->IsRegistered() || PrimitiveComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
        {
            continue;
        }

        const FVector ClosestPoint = PrimitiveComponent->Bounds.GetBox().GetClosestPointTo(Origin);
        if (CanSeeTargetPoint(*Target, ClosestPoint))
        {
            return true;
        }
    }
    return false;
}

// 임의 지점이 거리와 수평·수직 각도로 정의된 시야 영역 안인지 판정한다.
bool UCMAggressiveSightComponent::IsPointInsideSight(const FVector& Origin, const FVector& Forward, float HorizontalAngleDegrees, float VerticalAngleDegrees, float DistanceCm, const FVector& Point)
{
    const FVector ToPoint = Point - Origin;
    const float SafeDistance = FMath::Max(DistanceCm, 0.0f);
    if (ToPoint.SizeSquared() > FMath::Square(SafeDistance))
    {
        return false;
    }
    if (ToPoint.IsNearlyZero())
    {
        return true;
    }

    const FVector PlanarToPoint(ToPoint.X, ToPoint.Y, 0.0f);
    const float PlanarDistance = PlanarToPoint.Size();
    const float ElevationDegrees = FMath::RadiansToDegrees(FMath::Atan2(FMath::Abs(ToPoint.Z), PlanarDistance));
    const float SafeVerticalAngle = FMath::Clamp(VerticalAngleDegrees, 0.0f, 180.0f);
    if (ElevationDegrees > SafeVerticalAngle * 0.5f + UE_KINDA_SMALL_NUMBER)
    {
        return false;
    }

    if (PlanarDistance <= UE_KINDA_SMALL_NUMBER)
    {
        return true;
    }

    const float SafeHorizontalAngle = FMath::Clamp(HorizontalAngleDegrees, 0.0f, 360.0f);
    if (SafeHorizontalAngle >= 360.0f)
    {
        return true;
    }

    const FVector PlanarForward = Forward.GetSafeNormal2D();
    if (PlanarForward.IsNearlyZero())
    {
        return false;
    }

    const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(SafeHorizontalAngle * 0.5f));

    return FVector::DotProduct(PlanarToPoint / PlanarDistance, PlanarForward) >= MinimumDot - UE_KINDA_SMALL_NUMBER;
}

// 감지 결과가 플레이어 화면의 붉은 시야 효과로만 기여하도록 부모 시야를 구성한다.
void UCMAggressiveSightComponent::ApplyVisionSettings()
{
    ConfigureVision(HorizontalSightAngleDegrees, SightDistanceCm, 0.0f);
    SetVisionEyeHeightOffset(0.0f);
    SetVisionContribution(ECMVisionContribution::TintOnly);
    SetVisionTint(FLinearColor::Red, CMAggressiveSight::RedTintStrength);
    SetVisionActive(false);
}

// 서버가 플레이어 감지 상태를 계산해 시야 효과 활성 여부를 갱신한다.
void UCMAggressiveSightComponent::UpdateAuthoritySight()
{
    const FVector SightForward = GetSightForward();
    SetAimDirection(SightForward);

    const UWorld* World = GetWorld();
    const ACMGameState* GameState = World ? World->GetGameState<ACMGameState>() : nullptr;
    const ACMChimera* PlayerChimera = GameState ? GameState->SharedChimera : nullptr;
    const bool bPlayerSeen = IsValid(PlayerChimera) && CanSeeActor(PlayerChimera);
    SetVisionActive(bPlayerSeen);
}

// 대상 액터의 기준점이나 충돌 몸체 최근접점이 시야 영역과 차폐 검사를 모두 통과하는지 확인한다.
bool UCMAggressiveSightComponent::CanSeeTargetPoint(const AActor& Target, const FVector& TargetPoint) const
{
    return IsPointInsideSight(GetVisionOrigin(), GetSightForward(), HorizontalSightAngleDegrees, VerticalSightAngleDegrees, SightDistanceCm, TargetPoint) && HasClearSightTo(Target, TargetPoint);
}

// 소유자와 대상 지점 사이를 가로막는 월드 정적 장애물이 없는지 확인한다.
bool UCMAggressiveSightComponent::HasClearSightTo(const AActor& Target, const FVector& TargetPoint) const
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CMAggressiveSightOcclusion), false, GetOwner());
    QueryParams.AddIgnoredActor(&Target);

    FCollisionObjectQueryParams ObjectQueryParams;
    ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);

    return !World->LineTraceTestByObjectType(GetVisionOrigin(), TargetPoint, ObjectQueryParams, QueryParams);
}

FVector UCMAggressiveSightComponent::GetSightForward() const
{
    const FVector Forward = GetForwardVector();

    return bSightForwardReversed ? -Forward : Forward;
}
